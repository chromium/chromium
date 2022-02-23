// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeAnyValue;
using css_parsing_utils::ConsumeIfIdent;

CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsCondition(
    CSSParserTokenStream& stream,
    CSSParserImpl& parser) {
  stream.ConsumeWhitespace();
  CSSSupportsParser supports_parser(parser);
  return supports_parser.ConsumeSupportsCondition(stream);
}

// <supports-condition> = not <supports-in-parens>
//                   | <supports-in-parens> [ and <supports-in-parens> ]*
//                   | <supports-in-parens> [ or <supports-in-parens> ]*
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsCondition(
    CSSParserTokenStream& stream) {
  // not <supports-in-parens>
  stream.ConsumeWhitespace();
  if (ConsumeIfIdent(stream, "not"))
    return !ConsumeSupportsInParens(stream);

  // <supports-in-parens> [ and <supports-in-parens> ]*
  // | <supports-in-parens> [ or <supports-in-parens> ]*
  Result result = ConsumeSupportsInParens(stream);

  stream.ConsumeWhitespace();
  if (AtIdent(stream.Peek(), "and")) {
    stream.ConsumeWhitespace();
    while (ConsumeIfIdent(stream, "and")) {
      result = result & ConsumeSupportsInParens(stream);
      stream.ConsumeWhitespace();
    }
  } else if (AtIdent(stream.Peek(), "or")) {
    stream.ConsumeWhitespace();
    while (ConsumeIfIdent(stream, "or")) {
      result = result | ConsumeSupportsInParens(stream);
      stream.ConsumeWhitespace();
    }
  }

  return result;
}

bool CSSSupportsParser::IsSupportsInParens(const CSSParserToken& token) {
  // All three productions for <supports-in-parens> must start with either a
  // left parenthesis or a function.
  return token.GetType() == kLeftParenthesisToken ||
         token.GetType() == kFunctionToken;
}

bool CSSSupportsParser::IsEnclosedSupportsCondition(
    const CSSParserToken& first_token,
    const CSSParserToken& second_token) {
  return (first_token.GetType() == kLeftParenthesisToken) &&
         (AtIdent(second_token, "not") ||
          second_token.GetType() == kLeftParenthesisToken ||
          second_token.GetType() == kFunctionToken);
}

bool CSSSupportsParser::IsSupportsSelectorFn(
    const CSSParserToken& first_token,
    const CSSParserToken& second_token) {
  return (first_token.GetType() == kFunctionToken &&
          first_token.FunctionId() == CSSValueID::kSelector);
}

bool CSSSupportsParser::IsSupportsDecl(const CSSParserToken& first_token,
                                       const CSSParserToken& second_token) {
  return first_token.GetType() == kLeftParenthesisToken &&
         second_token.GetType() == kIdentToken;
}

bool CSSSupportsParser::IsSupportsFeature(const CSSParserToken& first_token,
                                          const CSSParserToken& second_token) {
  return IsSupportsSelectorFn(first_token, second_token) ||
         IsSupportsDecl(first_token, second_token);
}

bool CSSSupportsParser::IsGeneralEnclosed(const CSSParserToken& first_token) {
  return first_token.GetType() == kLeftParenthesisToken ||
         first_token.GetType() == kFunctionToken;
}

// <supports-in-parens> = ( <supports-condition> )
//                    | <supports-feature>
//                    | <general-enclosed>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsInParens(
    CSSParserTokenStream& stream) {
  CSSParserToken first_token = stream.Peek();
  if (!IsSupportsInParens(first_token))
    return Result::kParseFailure;

  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();

  // ( <supports-condition> )
  if (IsEnclosedSupportsCondition(first_token, stream.Peek())) {
    Result result = ConsumeSupportsCondition(stream);
    return stream.AtEnd() ? result : Result::kParseFailure;
  }

  // <supports-feature>
  if (IsSupportsFeature(first_token, stream.Peek())) {
    Result result = ConsumeSupportsFeature(first_token, stream);
    return stream.AtEnd() ? result : Result::kParseFailure;
  }

  // <general-enclosed>
  return ConsumeGeneralEnclosed(first_token, stream);
}

// <supports-feature> = <supports-selector-fn> | <supports-decl>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsFeature(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  // <supports-selector-fn>
  if (IsSupportsSelectorFn(first_token, stream.Peek()))
    return ConsumeSupportsSelectorFn(first_token, stream);

  // <supports-decl>
  return ConsumeSupportsDecl(first_token, stream);
}

// <supports-selector-fn> = selector( <complex-selector> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsSelectorFn(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  DCHECK(IsSupportsSelectorFn(first_token, stream.Peek()));
  auto block = stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();
  if (CSSSelectorParser::SupportsComplexSelector(block, parser_.GetContext()))
    return Result::kSupported;
  return Result::kUnsupported;
}

// <supports-decl> = ( <declaration> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsDecl(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  if (!IsSupportsDecl(first_token, stream.Peek()))
    return Result::kParseFailure;
  if (parser_.ConsumeSupportsDeclaration(stream))
    return Result::kSupported;
  return Result::kUnsupported;
}

// <general-enclosed> = [ <function-token> <any-value> ) ]
//                  | ( <ident> <any-value> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeGeneralEnclosed(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  if (IsGeneralEnclosed(first_token)) {
    auto block = stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();
    // TODO(crbug.com/1269284): We should allow empty values here.
    if (!ConsumeAnyValue(block) || !block.AtEnd())
      return Result::kParseFailure;

    stream.ConsumeWhitespace();
    return Result::kUnsupported;
  }
  return Result::kParseFailure;
}

}  // namespace blink
