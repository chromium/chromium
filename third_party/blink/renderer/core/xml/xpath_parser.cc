/*
 * Copyright 2005 Maksim Orlovich <maksim@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xpath_parser.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/xml/xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_grammar_generated.h"
#include "third_party/blink/renderer/core/xml/xpath_path.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {
namespace xpath {

using xpathyy::YyParser;
using TokenType = xpathyy::YyParser::token;

Parser* Parser::current_parser_ = nullptr;

enum XMLCat { kNameStart, kNameCont, kNotPartOfName };

typedef HashMap<String, Step::Axis> AxisNamesMap;

static XMLCat CharCat(UChar a_char) {
  // might need to add some special cases from the XML spec.

  if (a_char == '_')
    return kNameStart;

  if (a_char == '.' || a_char == '-')
    return kNameCont;
  WTF::unicode::CharCategory category = WTF::unicode::Category(a_char);
  if (category &
      (WTF::unicode::kLetter_Uppercase | WTF::unicode::kLetter_Lowercase |
       WTF::unicode::kLetter_Other | WTF::unicode::kLetter_Titlecase |
       WTF::unicode::kNumber_Letter))
    return kNameStart;
  if (category &
      (WTF::unicode::kMark_NonSpacing | WTF::unicode::kMark_SpacingCombining |
       WTF::unicode::kMark_Enclosing | WTF::unicode::kLetter_Modifier |
       WTF::unicode::kNumber_DecimalDigit))
    return kNameCont;
  return kNotPartOfName;
}

static void SetUpAxisNamesMap(AxisNamesMap& axis_names) {
  struct AxisName {
    const char* name;
    Step::Axis axis;
  };
  const AxisName kAxisNameList[] = {
      {"ancestor", Step::kAncestorAxis},
      {"ancestor-or-self", Step::kAncestorOrSelfAxis},
      {"attribute", Step::kAttributeAxis},
      {"child", Step::kChildAxis},
      {"descendant", Step::kDescendantAxis},
      {"descendant-or-self", Step::kDescendantOrSelfAxis},
      {"following", Step::kFollowingAxis},
      {"following-sibling", Step::kFollowingSiblingAxis},
      {"namespace", Step::kNamespaceAxis},
      {"parent", Step::kParentAxis},
      {"preceding", Step::kPrecedingAxis},
      {"preceding-sibling", Step::kPrecedingSiblingAxis},
      {"self", Step::kSelfAxis}};
  for (const auto& axis_name : kAxisNameList)
    axis_names.Set(axis_name.name, axis_name.axis);
}

static bool IsAxisName(const String& name, Step::Axis& type) {
  DEFINE_STATIC_LOCAL(AxisNamesMap, axis_names, ());

  if (axis_names.empty())
    SetUpAxisNamesMap(axis_names);

  AxisNamesMap::iterator it = axis_names.find(name);
  if (it == axis_names.end())
    return false;
  type = it->value;
  return true;
}

static bool IsNodeTypeName(const String& name) {
  DEFINE_STATIC_LOCAL(HashSet<String>, node_type_names,
                      ({
                          "comment", "text", "processing-instruction", "node",
                      }));
  return node_type_names.Contains(name);
}

// Returns whether the current token can possibly be a binary operator, given
// the previous token. Necessary to disambiguate some of the operators
// (* (multiply), div, and, or, mod) in the [32] Operator rule
// (check http://www.w3.org/TR/xpath#exprlex).
bool Parser::IsBinaryOperatorContext() const {
  switch (last_token_type_) {
    case 0:
    case '@':
    case TokenType::kAxisName:
    case '(':
    case '[':
    case ',':
    case TokenType::kAnd:
    case TokenType::kOr:
    case TokenType::kMulOp:
    case '/':
    case TokenType::kSlashSlash:
    case '|':
    case TokenType::kPlus:
    case TokenType::kMinus:
    case TokenType::kEqOp:
    case TokenType::kRelOp:
      return false;
    default:
      return true;
  }
}

// See https://www.w3.org/TR/1999/REC-xpath-19991116/#NT-ExprWhitespace .
void Parser::SkipWS() {
  while (next_pos_ < data_.length() && IsXMLSpace(data_[next_pos_]))
    ++next_pos_;
}

Token Parser::MakeTokenAndAdvance(int code, int advance) {
  next_pos_ += advance;
  return Token(code);
}

Token Parser::MakeTokenAndAdvance(int code,
                                  NumericOp::Opcode val,
                                  int advance) {
  next_pos_ += advance;
  return Token(code, val);
}

Token Parser::MakeTokenAndAdvance(int code, EqTestOp::Opcode val, int advance) {
  next_pos_ += advance;
  return Token(code, val);
}

// Returns next char if it's there and interesting, 0 otherwise
char Parser::PeekAheadHelper() {
  if (next_pos_ + 1 >= data_.length())
    return 0;
  UChar next = data_[next_pos_ + 1];
  if (next >= 0xff)
    return 0;
  return next;
}

char Parser::PeekCurHelper() {
  if (next_pos_ >= data_.length())
    return 0;
  UChar next = data_[next_pos_];
  if (next >= 0xff)
    return 0;
  return next;
}

Token Parser::LexString() {
  UChar delimiter = data_[next_pos_];
  int start_pos = next_pos_ + 1;

  for (next_pos_ = start_pos; next_pos_ < data_.length(); ++next_pos_) {
    if (data_[next_pos_] == delimiter) {
      String value = data_.Substring(start_pos, next_pos_ - start_pos);
      if (value.IsNull())
        value = "";
      ++next_pos_;  // Consume the char.
      return Token(TokenType::kLiteral, value);
    }
  }

  // Ouch, went off the end -- report error.
  return Token(TokenType::kXPathError);
}

Token Parser::LexNumber() {
  int start_pos = next_pos_;
  bool seen_dot = false;

  // Go until end or a non-digits character.
  for (; next_pos_ < data_.length(); ++next_pos_) {
    UChar a_char = data_[next_pos_];
    if (a_char >= 0xff)
      break;

    if (a_char < '0' || a_char > '9') {
      if (a_char == '.' && !seen_dot)
        seen_dot = true;
      else
        break;
    }
  }

  return Token(TokenType::kNumber,
               data_.Substring(start_pos, next_pos_ - start_pos));
}

bool Parser::LexNCName(String& name) {
  int start_pos = next_pos_;
  if (next_pos_ >= data_.length())
    return false;

  if (CharCat(data_[next_pos_]) != kNameStart)
    return false;

  // Keep going until we get a character that's not good for names.
  for (; next_pos_ < data_.length(); ++next_pos_) {
    if (CharCat(data_[next_pos_]) == kNotPartOfName)
      break;
  }

  name = data_.Substring(start_pos, next_pos_ - start_pos);
  return true;
}

bool Parser::LexQName(String& name) {
  String n1;
  if (!LexNCName(n1))
    return false;

  SkipWS();

  // If the next character is :, what we just got it the prefix, if not,
  // it's the whole thing.
  if (PeekAheadHelper() != ':') {
    name = n1;
    return true;
  }

  String n2;
  if (!LexNCName(n2))
    return false;

  name = n1 + ":" + n2;
  return true;
}

Token Parser::NextTokenInternal() {
  SkipWS();

  if (next_pos_ >= data_.length())
    return Token(0);

  char code = PeekCurHelper();
  switch (code) {
    case '(':
    case ')':
    case '[':
    case ']':
    case '@':
    case ',':
    case '|':
      return MakeTokenAndAdvance(code);
    case '\'':
    case '\"':
      return LexString();
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return LexNumber();
    case '.': {
      char next = PeekAheadHelper();
      if (next == '.')
        return MakeTokenAndAdvance(TokenType::kDotDot, 2);
      if (next >= '0' && next <= '9')
        return LexNumber();
      return MakeTokenAndAdvance('.');
    }
    case '/':
      if (PeekAheadHelper() == '/')
        return MakeTokenAndAdvance(TokenType::kSlashSlash, 2);
      return MakeTokenAndAdvance('/');
    case '+':
      return MakeTokenAndAdvance(TokenType::kPlus);
    case '-':
      return MakeTokenAndAdvance(TokenType::kMinus);
    case '=':
      return MakeTokenAndAdvance(TokenType::kEqOp, EqTestOp::kOpcodeEqual);
    case '!':
      if (PeekAheadHelper() == '=') {
        return MakeTokenAndAdvance(TokenType::kEqOp, EqTestOp::kOpcodeNotEqual,
                                   2);
      }
      return Token(TokenType::kXPathError);
    case '<':
      if (PeekAheadHelper() == '=') {
        return MakeTokenAndAdvance(TokenType::kRelOp,
                                   EqTestOp::kOpcodeLessOrEqual, 2);
      }
      return MakeTokenAndAdvance(TokenType::kRelOp, EqTestOp::kOpcodeLessThan);
    case '>':
      if (PeekAheadHelper() == '=') {
        return MakeTokenAndAdvance(TokenType::kRelOp,
                                   EqTestOp::kOpcodeGreaterOrEqual, 2);
      }
      return MakeTokenAndAdvance(TokenType::kRelOp,
                                 EqTestOp::kOpcodeGreaterThan);
    case '*':
      if (IsBinaryOperatorContext())
        return MakeTokenAndAdvance(TokenType::kMulOp, NumericOp::kOP_Mul);
      ++next_pos_;
      return Token(TokenType::kNameTest, "*");
    case '$': {  // $ QName
      next_pos_++;
      String name;
      if (!LexQName(name))
        return Token(TokenType::kXPathError);
      // DOM XPath API doesn't support any variables.
      if (use_counter_) {
        UseCounter::Count(use_counter_,
                          WebFeature::kXPathMissingVariableParsed);
      }
      return Token(TokenType::kVariableReference, name);
    }
  }

  String name;
  if (!LexNCName(name))
    return Token(TokenType::kXPathError);

  SkipWS();
  // If we're in an operator context, check for any operator names
  if (IsBinaryOperatorContext()) {
    if (name == "and")  // ### hash?
      return Token(TokenType::kAnd);
    if (name == "or")
      return Token(TokenType::kOr);
    if (name == "mod")
      return Token(TokenType::kMulOp, NumericOp::kOP_Mod);
    if (name == "div")
      return Token(TokenType::kMulOp, NumericOp::kOP_Div);
  }

  // See whether we are at a :
  if (PeekCurHelper() == ':') {
    next_pos_++;
    // Any chance it's an axis name?
    if (PeekCurHelper() == ':') {
      next_pos_++;

      // It might be an axis name.
      Step::Axis axis;
      if (IsAxisName(name, axis))
        return Token(TokenType::kAxisName, axis);
      // Ugh, :: is only valid in axis names -> error
      return Token(TokenType::kXPathError);
    }

    // Seems like this is a fully qualified qname, or perhaps the * modified
    // one from NameTest
    SkipWS();
    if (PeekCurHelper() == '*') {
      next_pos_++;
      return Token(TokenType::kNameTest, name + ":*");
    }

    // Make a full qname.
    String n2;
    if (!LexNCName(n2))
      return Token(TokenType::kXPathError);

    name = name + ":" + n2;
  }

  SkipWS();
  if (PeekCurHelper() == '(') {
    // Note: we don't swallow the ( here!

    // Either node type of function name
    if (IsNodeTypeName(name)) {
      if (name == "processing-instruction")
        return Token(TokenType::kPI, name);

      return Token(TokenType::kNodeType, name);
    }
    // Must be a function name.
    return Token(TokenType::kFunctionName, name);
  }

  // At this point, it must be NAMETEST.
  return Token(TokenType::kNameTest, name);
}

Token Parser::NextToken() {
  Token to_ret = NextTokenInternal();
  last_token_type_ = to_ret.type;
  return to_ret;
}

Parser::Parser(UseCounter* use_counter) : use_counter_(use_counter) {
  Reset(String());
}

Parser::~Parser() = default;

void Parser::Reset(const String& data) {
  next_pos_ = 0;
  data_ = data;
  last_token_type_ = 0;

  top_expr_ = nullptr;
  got_namespace_error_ = false;
}

int Parser::Lex(void* data) {
  auto* yylval = static_cast<YyParser::semantic_type*>(data);
  Token tok = NextToken();

  switch (tok.type) {
    case TokenType::kAxisName:
      yylval->build<Step::Axis>() = tok.axis;
      break;
    case TokenType::kMulOp:
      yylval->build<NumericOp::Opcode>() = tok.numop;
      break;
    case TokenType::kRelOp:
    case TokenType::kEqOp:
      yylval->build<EqTestOp::Opcode>() = tok.eqop;
      break;
    case TokenType::kNodeType:
    case TokenType::kPI:
    case TokenType::kFunctionName:
    case TokenType::kLiteral:
    case TokenType::kVariableReference:
    case TokenType::kNumber:
    case TokenType::kNameTest:
      yylval->build<String>() = String(tok.str);
      break;
  }

  return tok.type;
}

bool Parser::ExpandQName(const String& q_name,
                         AtomicString& local_name,
                         AtomicString& namespace_uri) {
  wtf_size_t colon = q_name.find(':');
  if (colon != kNotFound) {
    if (!resolver_)
      return false;
    String prefix = q_name.Left(colon);
    v8::TryCatch try_catch(resolver_->GetIsolate());
    try_catch.SetVerbose(true);  // Print exceptions to console.
    String uri;
    if (!resolver_->lookupNamespaceURI(nullptr, prefix).To(&uri))
      return false;
    if (uri.IsNull())
      return false;
    namespace_uri = AtomicString(uri);
    local_name = AtomicString(q_name.Substring(colon + 1));
  } else {
    local_name = AtomicString(q_name);
  }

  return true;
}

Expression* Parser::ParseStatement(const String& statement,
                                   V8XPathNSResolver* resolver,
                                   ExceptionState& exception_state) {
  Reset(statement);

  resolver_ = resolver;

  Parser* old_parser = current_parser_;
  current_parser_ = this;
  int parse_error = YyParser(this).parse();
  current_parser_ = old_parser;

  if (parse_error) {
    top_expr_ = nullptr;

    if (got_namespace_error_)
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNamespaceError,
          "The string '" + statement + "' contains unresolvable namespaces.");
    else
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The string '" + statement + "' is not a valid XPath expression.");
    return nullptr;
  }
  Expression* result = top_expr_;
  top_expr_ = nullptr;

  return result;
}

}  // namespace xpath
}  // namespace blink
