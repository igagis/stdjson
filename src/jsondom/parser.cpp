#include "parser.hpp"

#include <sstream>

#include <utki/string.hpp>

#include "malformed_json_error.hpp"

using namespace jsondom;

void parser::throw_malformed_json_error(char unexpected_char, const std::string& state_name){
	std::stringstream ss;
	ss << "unexpected character '" << unexpected_char << "' encountered while in " << state_name << " state, line = " << this->line;
	throw malformed_json_error(ss.str());
}

void parser::feed(utki::span<const char> data){
	for(auto i = data.begin(), e = data.end(); i != e; ++i){
		ASSERT(!this->state_stack.empty())
		switch(this->state_stack.back()){
			case state::idle:
				this->parse_idle(i, e);
				break;
			case state::object:
				this->parse_object(i, e);
				break;
			case state::array:
				this->parse_array(i, e);
				break;
			case state::key:
				this->parse_key(i, e);
				break;
			case state::colon:
				this->parse_colon(i, e);
				break;
			case state::value:
				this->parse_value(i, e);
				break;
			case state::comma:
				this->parse_comma(i, e);
				break;
			case state::string:
				this->parse_string(i, e);
				break;
			case state::boolean_or_null:
				this->parse_boolean_or_null(i, e);
				break;
		}
		if(i == e){
			return;
		}
	}
}

void parser::parse_idle(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case '{':
				this->state_stack.push_back(state::object);
				this->on_object_start();
				return;
			default:
				this->throw_malformed_json_error(*i, "idle");
				break;
		}
	}
}

void parser::parse_object(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case '}':
				this->state_stack.pop_back();
				this->on_object_end();
				return;
			case '"':
				this->state_stack.push_back(state::key);
				return;
			default:
				this->throw_malformed_json_error(*i, "object");
				break;
		}
	}
}

void parser::parse_key(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
			default:
				this->buf.push_back(*i);
				break;
			case '"':
				this->state_stack.pop_back();
				this->on_key_parsed(utki::make_span(this->buf));
				this->buf.clear();
				this->state_stack.push_back(state::colon);
				return;
		}
	}
}

void parser::parse_colon(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case ':':
				this->state_stack.pop_back();
				this->state_stack.push_back(state::value);
				return;
			default:
				this->throw_malformed_json_error(*i, "colon");
				break;
		}
	}
}

void parser::parse_value(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case '{':
				this->state_stack.pop_back();
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::object);
				this->on_object_start();
				return;
			case '[':
				this->on_array_start();
				this->state_stack.pop_back();
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::array);
				return;
			case '"':
				this->state_stack.pop_back();
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::string);
				return;
			case 't':
			case 'f':
			case 'n':
				ASSERT(this->buf.empty())
				this->buf.push_back(*i);
				this->state_stack.pop_back();
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::boolean_or_null);
				return;
			default:
				this->throw_malformed_json_error(*i, "value");
				break;
		}
	}
}

void parser::parse_array(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case '{':
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::object);
				this->on_object_start();
				return;
			case '[':
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::array);
				this->on_array_start();
				return;
			case '"':
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::string);
				return;
			case ']':
				this->state_stack.pop_back();
				this->on_array_end();
				return;
			case 't':
			case 'f':
			case 'n':
				ASSERT(this->buf.empty())
				this->buf.push_back(*i);
				this->state_stack.push_back(state::comma);
				this->state_stack.push_back(state::boolean_or_null);
				return;
			default:
				this->throw_malformed_json_error(*i, "array");
				break;
		}
	}
}

void parser::parse_string(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		switch(*i){
			case '\n':
				++this->line;
			default:
				this->buf.push_back(*i);
				break;
			case '"':
				this->state_stack.pop_back();
				this->on_string_parsed(utki::make_span(this->buf));
				this->buf.clear();
				return;
		}
	}
}

void parser::parse_comma(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		ASSERT(this->buf.empty())
		switch(*i){
			case '\n':
				++this->line;
			case ' ':
			case '\r':
			case '\t':
				break;
			case ',':
				this->state_stack.pop_back();
				return;
			case '}':
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				if(this->state_stack.back() != state::object){
					this->throw_malformed_json_error(*i, "comma");
				}
				this->state_stack.pop_back();
				this->on_object_end();
				return;
			case ']':
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				if(this->state_stack.back() != state::array){
					this->throw_malformed_json_error(*i, "comma");
				}
				this->state_stack.pop_back();
				this->on_array_end();
				return;
			default:
				this->throw_malformed_json_error(*i, "comma");
				break;
		}
	}
}

void parser::parse_boolean_or_null(utki::span<char>::const_iterator& i, utki::span<char>::const_iterator& e){
	for(; i != e; ++i){
		switch(*i){
			case '\n':
				++this->line;
			case '\r':
			case '\t':
			case ' ':
				this->notify_boolean_or_null_parsed();
				this->state_stack.pop_back();
				return;
			case ',':
				this->notify_boolean_or_null_parsed();
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				ASSERT(this->state_stack.back() == state::comma)
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				return;
			case ']':
				this->notify_boolean_or_null_parsed();
				this->on_array_end();
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				ASSERT(this->state_stack.back() == state::comma)
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				ASSERT(this->state_stack.back() == state::array)
				this->state_stack.pop_back();
				ASSERT(!this->state_stack.empty())
				return;
			default:
				this->buf.push_back(*i);
				break;
		}
	}
}

void parser::notify_boolean_or_null_parsed(){
	auto s = utki::make_string(this->buf);
	this->buf.clear();
	if(s == "true"){
		this->on_boolean_parsed(true);
	}else if(s == "false"){
		this->on_boolean_parsed(false);
	}else if(s == "null"){
		this->on_null_parsed();
	}else{
		std::stringstream ss;
		ss << "unexpected string (" << s << ") encountered while parsing boolean or null at line " << this->line;
		throw malformed_json_error(ss.str());
	}
}