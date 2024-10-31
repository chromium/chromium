// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
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
  if (ConsumeSupportsFeature(stream)) {
    return Result::kSupported;
  }

  // <general-enclosed>
  if (ConsumeGeneralEnclosed(stream)) {
    // <general-enclosed> evaluates to kUnsupported, even when parsed
    // successfully.
    return Result::kUnsupported;
  }

  return Result::kParseFailure;
}

// https://drafts.csswg.org/css-conditional-4/#at-supports-ext
// <supports-feature> = <supports-selector-fn> | <supports-font-tech-fn>
//                    | <supports-font-format-fn> | <supports-at-rule-fn>
//                    | <supports-decl>
//
// <supports-at-rule-fn> is currently only documented here:
// https://github.com/w3c/csswg-drafts/issues/2463#issuecomment-1016720310
bool CSSSupportsParser::ConsumeSupportsFeature(CSSParserTokenStream& stream) {
  // <supports-selector-fn>
  if (ConsumeSupportsSelectorFn(stream)) {
    return true;
  }
  // <supports-font-tech-fn>
  if (ConsumeFontTechFn(stream)) {
    return true;
  }
  // <supports-font-format-fn>
  if (ConsumeFontFormatFn(stream)) {
    return true;
  }
  // <supports-at-rule-fn>
  if (ConsumeAtRuleFn(stream)) {
    return true;
  }
  if (parser_.GetMode() == CSSParserMode::kUASheetMode) {
    if (ConsumeBlinkFeatureFn(stream)) {
      return true;
    }
  }
  // <supports-decl>
  return ConsumeSupportsDecl(stream);
}

// <supports-selector-fn> = selector( <complex-selector> )
bool CSSSupportsParser::ConsumeSupportsSelectorFn(
    CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kSelector) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (CSSSelectorParser::SupportsComplexSelector(stream,
                                                 parser_.GetContext()) &&
      guard.Release()) {
    stream.ConsumeWhitespace();
    return true;
  }
  return false;
}

bool CSSSupportsParser::ConsumeFontFormatFn(CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kFontFormat) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontFormatIdent(stream);

  if (consumed_value &&
      css_parsing_utils::IsSupportedKeywordFormat(
          consumed_value->GetValueID()) &&
      guard.Release()) {
    stream.ConsumeWhitespace();
    return true;
  }

  return false;
}

bool CSSSupportsParser::ConsumeFontTechFn(CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kFontTech) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  CSSIdentifierValue* consumed_value =
      css_parsing_utils::ConsumeFontTechIdent(stream);

  if (consumed_value &&
      css_parsing_utils::IsSupportedKeywordTech(consumed_value->GetValueID()) &&
      guard.Release()) {
    stream.ConsumeWhitespace();
    return true;
  }

  return false;
}

// <supports-at-rule-fn> = at-rule( <at-rule> [ ; <descriptor> : <value> ]? )
bool CSSSupportsParser::ConsumeAtRuleFn(CSSParserTokenStream& stream) {
  if (!RuntimeEnabledFeatures::CSSSupportsAtRuleFunctionEnabled()) {
    return false;
  }

  if (stream.Peek().FunctionId() != CSSValueID::kAtRule) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() != kAtKeywordToken) {
    return false;
  }
  CSSParserToken name_token = stream.ConsumeIncludingWhitespace();
  const StringView name = name_token.Value();
  const CSSAtRuleID at_rule_id = CssAtRuleID(name);
  if (at_rule_id == CSSAtRuleID::kCSSAtRuleInvalid) {
    return false;
  }

  if (stream.AtEnd()) {
    return guard.Release();
  }

  StyleRule::RuleType rule_type;
  switch (at_rule_id) {
    case CSSAtRuleID::kCSSAtRuleInvalid:
      NOTREACHED();
    case CSSAtRuleID::kCSSAtRuleViewTransition:
      rule_type = StyleRule::kViewTransition;
      break;
    case CSSAtRuleID::kCSSAtRuleContainer:
      rule_type = StyleRule::kContainer;
      break;
    case CSSAtRuleID::kCSSAtRuleMedia:
      rule_type = StyleRule::kMedia;
      break;
    case CSSAtRuleID::kCSSAtRuleSupports:
      rule_type = StyleRule::kSupports;
      break;
    case CSSAtRuleID::kCSSAtRuleStartingStyle:
      rule_type = StyleRule::kStartingStyle;
      break;
    case CSSAtRuleID::kCSSAtRuleFontFace:
      rule_type = StyleRule::kFontFace;
      break;
    case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
      rule_type = StyleRule::kFontPaletteValues;
      break;
    case CSSAtRuleID::kCSSAtRuleFontFeatureValues:
      rule_type = StyleRule::kFontFeatureValues;
      break;
    case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
    case CSSAtRuleID::kCSSAtRuleKeyframes:
      rule_type = StyleRule::kKeyframes;
      break;
    case CSSAtRuleID::kCSSAtRuleLayer:
      rule_type = StyleRule::kLayerBlock;
      break;
    case CSSAtRuleID::kCSSAtRulePage:
      rule_type = StyleRule::kPage;
      break;
    case CSSAtRuleID::kCSSAtRuleProperty:
      rule_type = StyleRule::kProperty;
      break;
    case CSSAtRuleID::kCSSAtRuleScope:
      rule_type = StyleRule::kScope;
      break;
    case CSSAtRuleID::kCSSAtRuleCounterStyle:
      rule_type = StyleRule::kCounterStyle;
      break;
    case CSSAtRuleID::kCSSAtRuleFunction:
      rule_type = StyleRule::kFunction;
      break;
    case CSSAtRuleID::kCSSAtRuleMixin:
      rule_type = StyleRule::kMixin;
      break;
    case CSSAtRuleID::kCSSAtRuleApplyMixin:
      rule_type = StyleRule::kApplyMixin;
      break;
    case CSSAtRuleID::kCSSAtRulePositionTry:
      rule_type = StyleRule::kPositionTry;
      break;
    case CSSAtRuleID::kCSSAtRuleCharset:
      rule_type = StyleRule::kCharset;
      break;
    case CSSAtRuleID::kCSSAtRuleImport:
      rule_type = StyleRule::kImport;
      break;
    case CSSAtRuleID::kCSSAtRuleNamespace:
      rule_type = StyleRule::kNamespace;
      break;
    case CSSAtRuleID::kCSSAtRuleStylistic:
    case CSSAtRuleID::kCSSAtRuleStyleset:
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
    case CSSAtRuleID::kCSSAtRuleSwash:
    case CSSAtRuleID::kCSSAtRuleOrnaments:
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      rule_type = StyleRule::kFontFeature;
      break;
    case CSSAtRuleID::kCSSAtRuleTopLeftCorner:
    case CSSAtRuleID::kCSSAtRuleTopLeft:
    case CSSAtRuleID::kCSSAtRuleTopCenter:
    case CSSAtRuleID::kCSSAtRuleTopRight:
    case CSSAtRuleID::kCSSAtRuleTopRightCorner:
    case CSSAtRuleID::kCSSAtRuleBottomLeftCorner:
    case CSSAtRuleID::kCSSAtRuleBottomLeft:
    case CSSAtRuleID::kCSSAtRuleBottomCenter:
    case CSSAtRuleID::kCSSAtRuleBottomRight:
    case CSSAtRuleID::kCSSAtRuleBottomRightCorner:
    case CSSAtRuleID::kCSSAtRuleLeftTop:
    case CSSAtRuleID::kCSSAtRuleLeftMiddle:
    case CSSAtRuleID::kCSSAtRuleLeftBottom:
    case CSSAtRuleID::kCSSAtRuleRightTop:
    case CSSAtRuleID::kCSSAtRuleRightMiddle:
    case CSSAtRuleID::kCSSAtRuleRightBottom:
      rule_type = StyleRule::kPageMargin;
      break;
  };

  // Parse an optional descriptor.
  if (stream.Peek().GetType() != kSemicolonToken) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();

  // The descriptor ID.
  if (stream.Peek().GetType() != kIdentToken) {
    return false;
  }
  AtRuleDescriptorID descriptor_id = stream.Peek().ParseAsAtRuleDescriptorID();
  if (descriptor_id == AtRuleDescriptorID::Invalid) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();

  // Colon.
  if (stream.Peek().GetType() != kColonToken) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();

  // The descriptor value.
  HeapVector<CSSPropertyValue, 64> parsed_descriptors;
  bool ok = AtRuleDescriptorParser::ParseDescriptorValue(
      rule_type, descriptor_id, stream, *parser_.GetContext(),
      parsed_descriptors);

  return ok && guard.Release();
}

// <supports-decl> = ( <declaration> )
bool CSSSupportsParser::ConsumeSupportsDecl(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() == kIdentToken &&
      parser_.ConsumeSupportsDeclaration(stream) && guard.Release()) {
    stream.ConsumeWhitespace();
    return true;
  }
  return false;
}

// <general-enclosed> = [ <function-token> <any-value>? ) ]
//                  | ( <any-value>? )
bool CSSSupportsParser::ConsumeGeneralEnclosed(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken &&
      stream.Peek().GetType() != kFunctionToken) {
    return false;
  }

  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  ConsumeAnyValue(stream);
  if (guard.Release()) {
    stream.ConsumeWhitespace();
    return true;
  }
  return false;
}

bool CSSSupportsParser::ConsumeBlinkFeatureFn(CSSParserTokenStream& stream) {
  if (stream.Peek().FunctionId() != CSSValueID::kBlinkFeature) {
    return false;
  }
  CSSParserTokenStream::RestoringBlockGuard guard(stream);
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() == kIdentToken) {
    const CSSParserToken& feature_name = stream.ConsumeIncludingWhitespace();
    if (RuntimeEnabledFeatures::IsFeatureEnabledFromString(
            feature_name.Value().Utf8()) &&
        guard.Release()) {
      stream.ConsumeWhitespace();
      return true;
    }
  }
  return false;
}

}  // namespace blink
