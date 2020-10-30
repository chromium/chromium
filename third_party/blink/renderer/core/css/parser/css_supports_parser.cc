// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

namespace {

// The result kUnknown must be converted to 'false' if passed to a context
// which requires a boolean value.
// TODO(crbug.com/1052274): This is supposed to happen at the top-level,
// but currently happens on ConsumeGeneralEnclosed's result.
CSSSupportsParser::Result EvalUnknown(CSSSupportsParser::Result result) {
  return result == CSSSupportsParser::Result::kUnknown
             ? CSSSupportsParser::Result::kUnsupported
             : result;
}

// https://drafts.csswg.org/css-syntax/#typedef-any-value
bool IsNextTokenAllowedForAnyValue(CSSParserTokenRange& range) {
  switch (range.Peek().GetType()) {
    case kBadStringToken:
    case kEOFToken:
    case kBadUrlToken:
      return false;
    case kRightParenthesisToken:
    case kRightBracketToken:
    case kRightBraceToken:
      return range.Peek().GetBlockType() == CSSParserToken::kBlockEnd;
    default:
      return true;
  }
}

// https://drafts.csswg.org/css-syntax/#typedef-any-value
bool ConsumeAnyValue(CSSParserTokenRange& range) {
  DCHECK(!range.AtEnd());
  while (IsNextTokenAllowedForAnyValue(range))
    range.Consume();
  return range.AtEnd();
}

}  // namespace

CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsCondition(
    CSSParserTokenStream& stream,
    CSSParserImpl& parser) {
  stream.ConsumeWhitespace();
  CSSSupportsParser supports_parser(parser);
  return supports_parser.ConsumeSupportsCondition(stream);
}

bool CSSSupportsParser::AtIdent(const CSSParserToken& token,
                                const char* ident) {
  return token.GetType() == kIdentToken &&
         EqualIgnoringASCIICase(token.Value(), ident);
}

bool CSSSupportsParser::ConsumeIfIdent(CSSParserTokenStream& stream,
                                       const char* ident) {
  if (!AtIdent(stream.Peek(), ident))
    return false;
  stream.ConsumeIncludingWhitespace();
  return true;
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
    return guard.AtEndOfBlock() ? result : Result::kParseFailure;
  }

  // <supports-feature>
  if (IsSupportsFeature(first_token, stream.Peek())) {
    Result result = ConsumeSupportsFeature(first_token, stream);
    return guard.AtEndOfBlock() ? result : Result::kParseFailure;
  }

  // <general-enclosed>
  //
  // TODO(crbug.com/1052274): Support kUnknown beyond this point.
  //
  // The result kUnknown is supposed to be evaluated at the top level, but
  // we have already shipped the behavior of evaluating it here, and Firefox
  // does the same thing.
  return EvalUnknown(ConsumeGeneralEnclosed(first_token, stream));
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
    // Note that <any-value> matches a sequence of one or more tokens, hence the
    // block-range can't be empty.
    // https://drafts.csswg.org/css-syntax-3/#typedef-any-value
    if (block.AtEnd() || !ConsumeAnyValue(block))
      return Result::kParseFailure;

    stream.ConsumeWhitespace();
    return Result::kUnknown;
  }
  return Result::kParseFailure;
}

}  // namespace blink
