// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_AT_RULE_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_AT_RULE_ID_H_

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class CSSParserContext;

enum class CSSAtRuleID {
  kCSSAtRuleInvalid,
  kCSSAtRuleCharset,
  kCSSAtRuleFontFace,
  kCSSAtRuleFontPaletteValues,
  kCSSAtRuleImport,
  kCSSAtRuleKeyframes,
  kCSSAtRuleLayer,
  kCSSAtRuleMedia,
  kCSSAtRuleNamespace,
  kCSSAtRulePage,
  kCSSAtRulePositionFallback,
  kCSSAtRuleProperty,
  kCSSAtRuleContainer,
  kCSSAtRuleCounterStyle,
  kCSSAtRuleScope,
  kCSSAtRuleScrollTimeline,
  kCSSAtRuleSupports,
  kCSSAtRuleTry,
  kCSSAtRuleWebkitKeyframes,
  // Font-feature-values related at-rule ids below:
  kCSSAtRuleAnnotation,
  kCSSAtRuleCharacterVariant,
  kCSSAtRuleFontFeatureValues,
  kCSSAtRuleOrnaments,
  kCSSAtRuleStylistic,
  kCSSAtRuleStyleset,
  kCSSAtRuleSwash
};

CSSAtRuleID CssAtRuleID(StringView name);

void CountAtRule(const CSSParserContext*, CSSAtRuleID);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_AT_RULE_ID_H_
