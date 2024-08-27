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

//
// Every non-static Consume function should:
//
//  1. Assume that the calling function already consumed whitespace.
//  2. Clean up trailing whitespace if a supported condition was consumed.
//  3. Otherwise, leave the stream untouched.
//

// <supports-condition> = not <supports-in-parens>
//                   | <supports-in-parens> [ and <supports-in-parens> ]*
//                   | <supports-in-parens> [ or <supports-in-parens> ]*
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsCondition(
    CSSParserTokenStream& stream) {
  // not <supports-in-parens>
  if (ConsumeIfIdent(stream, "not")) {
    return !ConsumeSupportsInParens(stream);
  }

  // <supports-in-parens> [ and <supports-in-parens> ]*
  // | <supports-in-parens> [ or <supports-in-parens> ]*
  Result result = ConsumeSupportsInParens(stream);

  if (AtIdent(stream.Peek(), "and")) {
    while (ConsumeIfIdent(stream, "and")) {
      result = result & ConsumeSupportsInParens(stream);
    }
  } else if (AtIdent(stream.Peek(), "or")) {
    while (ConsumeIfIdent(stream, "or")) {
      result = result | ConsumeSupportsInParens(stream);
    }
  }

  return result;
}

// <supports-in-parens> = ( <supports-condition> )
//                    | <supports-feature>
//                    | <general-enclosed>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsInParens(
    CSSParserTokenStream& stream) {
  // ( <supports-condition> )
  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    Result result = ConsumeSupportsCondition(stream);
    if (result == Result::kSupported && guard.Release()) {
      stream.ConsumeWhitespace();
      return result;
    }
    // Otherwise, fall through.
    //
    // Note that even when the result is kParseFailure, we still want to fall
    // through here, in case it's valid as <general-enclosed>. If it's not,
    // we'll gain back the kParseFailure at the end of this function.
  }

  // <supports-feature>
  if (Result result = ConsumeSupportsFeature(stream);
      result == Result::kSupported) {
    return result;
  }

  // <general-enclosed>
  if (Result result = ConsumeGeneralEnclosed(stream);
      result == Result::kSupported) {
    // TODO(andruud): This is confusing, use bool as ConsumeGeneralEnclosed's
    // return value. ConsumeGeneralEnclosed returning 'kSupported' means that it
    // successfully parsed as <general-enclosed>. However, <general-enclosed>
    // should evaluate to false, hence we return kUnsupported.
    return Result::kUnsupported;
  }

  return Result::kParseFailure;
}

// https://drafts.csswg.org/css-conditional-4/#at-supports-ext
// <supports-feature> = <supports-selector-fn> | <supports-font-tech-fn>
//                    | <supports-font-format-fn> | <supports-decl>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsFeature(
    CSSParserTokenStream& stream) {
  // <supports-selector-fn>
  if (Result result = ConsumeSupportsSelectorFn(stream);
      result == Result::kSupported) {
    return result;
  }
  // <supports-font-tech-fn>
  if (Result result = ConsumeFontTechFn(stream); result == Result::kSupported) {
    return result;
  }
  // <supports-font-format-fn>
  if (Result result = ConsumeFontFormatFn(stream);
      result == Result::kSupported) {
    return result;
  }
  if (parser_.GetMode() == CSSParserMode::kUASheetMode) {
    if (Result result = ConsumeBlinkFeatureFn(stream);
        result == Result::kSupported) {
      return result;
    }
  }
  // <supports-decl>
  return ConsumeSupportsDecl(stream);
}

// <supports-selector-fn> = selector( <complex-selector> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsSelectorFn(
    CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kSelector) {
    return Result::kUnsupported;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (CSSSelectorParser::SupportsComplexSelector(stream,
                                                 parser_.GetContext()) &&
      guard.Release()) {
    stream.ConsumeWhitespace();
    return Result::kSupported;
  }
  return Result::kUnsupported;
}

CSSSupportsParser::Result CSSSupportsParser::ConsumeFontFormatFn(
    CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kFontFormat) {
    return Result::kUnsupported;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  auto format_block = stream.ConsumeUntilPeekedTypeIs<>();

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontFormatIdent(format_block);

  if (consumed_value &&
      css_parsing_utils::IsSupportedKeywordFormat(
          consumed_value->GetValueID()) &&
      format_block.AtEnd() && guard.Release()) {
    stream.ConsumeWhitespace();
    return Result::kSupported;
  }

  return Result::kUnsupported;
}

CSSSupportsParser::Result CSSSupportsParser::ConsumeFontTechFn(
    CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kFontTech) {
    return Result::kUnsupported;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  auto technology_block = stream.ConsumeUntilPeekedTypeIs<>();

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontTechIdent(technology_block);

  if (consumed_value &&
      css_parsing_utils::IsSupportedKeywordTech(consumed_value->GetValueID()) &&
      technology_block.AtEnd() && guard.Release()) {
    stream.ConsumeWhitespace();
    return Result::kSupported;
  }

  return Result::kUnsupported;
}

// <supports-decl> = ( <declaration> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsDecl(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken) {
    return Result::kUnsupported;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() == kIdentToken &&
      parser_.ConsumeSupportsDeclaration(stream) && guard.Release()) {
    stream.ConsumeWhitespace();
    return Result::kSupported;
  }
  return Result::kUnsupported;
}

// <general-enclosed> = [ <function-token> <any-value>? ) ]
//                  | ( <any-value>? )
CSSSupportsParser::Result CSSSupportsParser::ConsumeGeneralEnclosed(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken &&
      stream.Peek().GetType() != kFunctionToken) {
    return Result::kUnsupported;
  }

  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  ConsumeAnyValue(stream);
  if (guard.Release()) {
    stream.ConsumeWhitespace();
    return Result::kSupported;
  }
  return Result::kUnsupported;
}

CSSSupportsParser::Result CSSSupportsParser::ConsumeBlinkFeatureFn(
    CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kBlinkFeature) {
    return Result::kUnsupported;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() == kIdentToken) {
    const CSSParserToken& feature_name = stream.ConsumeIncludingWhitespace();
    if (RuntimeEnabledFeatures::IsFeatureEnabledFromString(
            feature_name.Value().Utf8()) &&
        guard.Release()) {
      stream.ConsumeWhitespace();
      return Result::kSupported;
    }
  }
  return Result::kUnsupported;
}

}  // namespace blink
