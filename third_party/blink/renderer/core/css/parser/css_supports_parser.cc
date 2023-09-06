// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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
  if (ConsumeIfIdent(stream, "not")) {
    Result result = ConsumeSupportsInParens(stream);
    stream.ConsumeWhitespace();
    return !result;
  }

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

bool CSSSupportsParser::IsFontTechFn(const CSSParserToken& first_token,
                                     const CSSParserToken& second_token) {
  return (first_token.GetType() == kFunctionToken &&
          first_token.FunctionId() == CSSValueID::kFontTech);
}

bool CSSSupportsParser::IsFontFormatFn(const CSSParserToken& first_token,
                                       const CSSParserToken& second_token) {
  return (first_token.GetType() == kFunctionToken &&
          first_token.FunctionId() == CSSValueID::kFontFormat);
}

bool CSSSupportsParser::IsSupportsDecl(const CSSParserToken& first_token,
                                       const CSSParserToken& second_token) {
  return first_token.GetType() == kLeftParenthesisToken &&
         second_token.GetType() == kIdentToken;
}

bool CSSSupportsParser::IsSupportsFeature(const CSSParserToken& first_token,
                                          const CSSParserToken& second_token) {
  return IsSupportsSelectorFn(first_token, second_token) ||
         IsSupportsDecl(first_token, second_token) ||
         IsFontFormatFn(first_token, second_token) ||
         IsFontTechFn(first_token, second_token);
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
  if (!IsSupportsInParens(first_token)) {
    return Result::kParseFailure;
  }

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

// https://drafts.csswg.org/css-conditional-4/#at-supports-ext
// <supports-feature> = <supports-selector-fn> | <supports-font-tech-fn>
//                    | <supports-font-format-fn> | <supports-decl>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsFeature(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  // <supports-selector-fn>
  if (IsSupportsSelectorFn(first_token, stream.Peek())) {
    return ConsumeSupportsSelectorFn(first_token, stream);
  }

  // <supports-font-tech-fn>
  if (IsFontTechFn(first_token, stream.Peek())) {
    return ConsumeFontTechFn(first_token, stream);
  }

  // <supports-font-format-fn>
  if (IsFontFormatFn(first_token, stream.Peek())) {
    return ConsumeFontFormatFn(first_token, stream);
  }

  // <supports-decl>
  return ConsumeSupportsDecl(first_token, stream);
}

// <supports-selector-fn> = selector( <complex-selector> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsSelectorFn(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  DCHECK(IsSupportsSelectorFn(first_token, stream.Peek()));
  auto block = stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();
  if (CSSSelectorParser::SupportsComplexSelector(block, parser_.GetContext())) {
    return Result::kSupported;
  }
  return Result::kUnsupported;
}

CSSSupportsParser::Result CSSSupportsParser::ConsumeFontFormatFn(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  DCHECK(IsFontFormatFn(first_token, stream.Peek()));

  auto format_block = stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();

  // Parse errors inside the parentheses are treated as kUnsupported to
  // simulate parsing the font-tech function as <general-enclosed>. In
  // other words: even if this block is not understood as font-tech, it
  // can be parsed as <general-enclosed> and result in false.

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontFormatIdent(format_block);

  if (!consumed_value) {
    return Result::kUnsupported;
  }

  CSSSupportsParser::Result parse_result = Result::kUnsupported;

  parse_result =
      css_parsing_utils::IsSupportedKeywordFormat(consumed_value->GetValueID())
          ? Result::kSupported
          : Result::kUnsupported;

  format_block.ConsumeWhitespace();
  if (!format_block.AtEnd()) {
    return Result::kUnsupported;
  }

  return parse_result;
}

CSSSupportsParser::Result CSSSupportsParser::ConsumeFontTechFn(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  DCHECK(IsFontTechFn(first_token, stream.Peek()));
  auto technology_block =
      stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();

  // Parse errors inside the parentheses are treated as kUnsupported to
  // simulate parsing the font-tech function as <general-enclosed>. In
  // other words: even if this block is not understood as font-tech, it
  // can be parsed as <general-enclosed> and result in false.

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontTechIdent(technology_block);

  if (!consumed_value) {
    return Result::kUnsupported;
  }

  CSSSupportsParser::Result parse_result = Result::kUnsupported;

  parse_result =
      css_parsing_utils::IsSupportedKeywordTech(consumed_value->GetValueID())
          ? Result::kSupported
          : Result::kUnsupported;

  technology_block.ConsumeWhitespace();
  if (!technology_block.AtEnd()) {
    return Result::kUnsupported;
  }

  return parse_result;
}

// <supports-decl> = ( <declaration> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsDecl(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  if (!IsSupportsDecl(first_token, stream.Peek())) {
    return Result::kParseFailure;
  }
  if (parser_.ConsumeSupportsDeclaration(stream)) {
    return Result::kSupported;
  }

  // ConsumeSupportsDeclaration can leave the stream in various states,
  // see documentation near CSSParserImpl::ConsumeDeclaration.
  if (!stream.AtEnd()) {
    // If there are remaining tokens, then ConsumeSupportsDeclaration backed
    // out early due to a missing colon or invalid property.
    // This normally means it's either an invalid property (kUnsupported),
    // or some other unknown construct (also kUnsupported). However, the unknown
    // construct must not violate the rules of <general-enclosed>. If that
    // happens, it is instead a kParseFailure.
    CSSParserTokenRange remaining = stream.ConsumeUntilPeekedTypeIs<>();
    // TODO(crbug.com/1361240): This is the same check as
    // ConsumeGeneralEnclosed. It would be cleaner to just restart and actually
    // call that function.
    if (!ConsumeAnyValue(remaining) || !remaining.AtEnd()) {
      return Result::kParseFailure;
    }
  }

  return Result::kUnsupported;
}

// <general-enclosed> = [ <function-token> <any-value>? ) ]
//                  | ( <any-value>? )
CSSSupportsParser::Result CSSSupportsParser::ConsumeGeneralEnclosed(
    const CSSParserToken& first_token,
    CSSParserTokenStream& stream) {
  if (IsGeneralEnclosed(first_token)) {
    auto block = stream.ConsumeUntilPeekedTypeIs<kRightParenthesisToken>();
    block.ConsumeWhitespace();
    if (block.AtEnd()) {
      return Result::kUnsupported;
    }

    if (!ConsumeAnyValue(block) || !block.AtEnd()) {
      return Result::kParseFailure;
    }

    stream.ConsumeWhitespace();
    return Result::kUnsupported;
  }
  return Result::kParseFailure;
}

}  // namespace blink
