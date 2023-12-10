// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSAtRuleID CssAtRuleID(StringView name) {
  if (EqualIgnoringASCIICase(name, "view-transition")) {
    if (RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled()) {
      return CSSAtRuleID::kCSSAtRuleViewTransition;
    }
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "charset")) {
    return CSSAtRuleID::kCSSAtRuleCharset;
  }
  if (EqualIgnoringASCIICase(name, "font-face")) {
    return CSSAtRuleID::kCSSAtRuleFontFace;
  }
  if (EqualIgnoringASCIICase(name, "font-palette-values")) {
    return CSSAtRuleID::kCSSAtRuleFontPaletteValues;
  }
  if (EqualIgnoringASCIICase(name, "font-feature-values")) {
    return CSSAtRuleID::kCSSAtRuleFontFeatureValues;
  }
  if (EqualIgnoringASCIICase(name, "stylistic")) {
    return CSSAtRuleID::kCSSAtRuleStylistic;
  }
  if (EqualIgnoringASCIICase(name, "styleset")) {
    return CSSAtRuleID::kCSSAtRuleStyleset;
  }
  if (EqualIgnoringASCIICase(name, "character-variant")) {
    return CSSAtRuleID::kCSSAtRuleCharacterVariant;
  }
  if (EqualIgnoringASCIICase(name, "swash")) {
    return CSSAtRuleID::kCSSAtRuleSwash;
  }
  if (EqualIgnoringASCIICase(name, "ornaments")) {
    return CSSAtRuleID::kCSSAtRuleOrnaments;
  }
  if (EqualIgnoringASCIICase(name, "annotation")) {
    return CSSAtRuleID::kCSSAtRuleAnnotation;
  }
  if (EqualIgnoringASCIICase(name, "import")) {
    return CSSAtRuleID::kCSSAtRuleImport;
  }
  if (EqualIgnoringASCIICase(name, "keyframes")) {
    return CSSAtRuleID::kCSSAtRuleKeyframes;
  }
  if (EqualIgnoringASCIICase(name, "layer")) {
    return CSSAtRuleID::kCSSAtRuleLayer;
  }
  if (EqualIgnoringASCIICase(name, "media")) {
    return CSSAtRuleID::kCSSAtRuleMedia;
  }
  if (EqualIgnoringASCIICase(name, "namespace")) {
    return CSSAtRuleID::kCSSAtRuleNamespace;
  }
  if (EqualIgnoringASCIICase(name, "page")) {
    return CSSAtRuleID::kCSSAtRulePage;
  }
  if (EqualIgnoringASCIICase(name, "position-fallback")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled()) {
      return CSSAtRuleID::kCSSAtRulePositionFallback;
    }
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "property")) {
    return CSSAtRuleID::kCSSAtRuleProperty;
  }
  if (EqualIgnoringASCIICase(name, "container")) {
    return CSSAtRuleID::kCSSAtRuleContainer;
  }
  if (EqualIgnoringASCIICase(name, "counter-style")) {
    return CSSAtRuleID::kCSSAtRuleCounterStyle;
  }
  if (EqualIgnoringASCIICase(name, "scope")) {
    if (RuntimeEnabledFeatures::CSSScopeEnabled()) {
      return CSSAtRuleID::kCSSAtRuleScope;
    }
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "supports")) {
    return CSSAtRuleID::kCSSAtRuleSupports;
  }
  if (EqualIgnoringASCIICase(name, "try")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled()) {
      return CSSAtRuleID::kCSSAtRuleTry;
    }
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "starting-style")) {
    return CSSAtRuleID::kCSSAtRuleStartingStyle;
  }
  if (EqualIgnoringASCIICase(name, "-webkit-keyframes")) {
    return CSSAtRuleID::kCSSAtRuleWebkitKeyframes;
  }
  return CSSAtRuleID::kCSSAtRuleInvalid;
}

namespace {

absl::optional<WebFeature> AtRuleFeature(CSSAtRuleID rule_id) {
  switch (rule_id) {
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      return WebFeature::kCSSAtRuleAnnotation;
    case CSSAtRuleID::kCSSAtRuleViewTransition:
      return WebFeature::kCSSAtRuleViewTransition;
    case CSSAtRuleID::kCSSAtRuleCharset:
      return WebFeature::kCSSAtRuleCharset;
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      return WebFeature::kCSSAtRuleCharacterVariant;
    case CSSAtRuleID::kCSSAtRuleFontFace:
      return WebFeature::kCSSAtRuleFontFace;
    case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
      return WebFeature::kCSSAtRuleFontPaletteValues;
    case CSSAtRuleID::kCSSAtRuleFontFeatureValues:
      return WebFeature::kCSSAtRuleFontFeatureValues;
    case CSSAtRuleID::kCSSAtRuleImport:
      return WebFeature::kCSSAtRuleImport;
    case CSSAtRuleID::kCSSAtRuleKeyframes:
      return WebFeature::kCSSAtRuleKeyframes;
    case CSSAtRuleID::kCSSAtRuleLayer:
      return WebFeature::kCSSCascadeLayers;
    case CSSAtRuleID::kCSSAtRuleMedia:
      return WebFeature::kCSSAtRuleMedia;
    case CSSAtRuleID::kCSSAtRuleNamespace:
      return WebFeature::kCSSAtRuleNamespace;
    case CSSAtRuleID::kCSSAtRulePage:
      return WebFeature::kCSSAtRulePage;
    case CSSAtRuleID::kCSSAtRuleProperty:
      return WebFeature::kCSSAtRuleProperty;
    case CSSAtRuleID::kCSSAtRuleContainer:
      return WebFeature::kCSSAtRuleContainer;
    case CSSAtRuleID::kCSSAtRuleCounterStyle:
      return WebFeature::kCSSAtRuleCounterStyle;
    case CSSAtRuleID::kCSSAtRuleOrnaments:
      return WebFeature::kCSSAtRuleOrnaments;
    case CSSAtRuleID::kCSSAtRuleScope:
      return WebFeature::kCSSAtRuleScope;
    case CSSAtRuleID::kCSSAtRuleStartingStyle:
      return WebFeature::kCSSAtRuleStartingStyle;
    case CSSAtRuleID::kCSSAtRuleStyleset:
      return WebFeature::kCSSAtRuleStylistic;
    case CSSAtRuleID::kCSSAtRuleStylistic:
      return WebFeature::kCSSAtRuleStylistic;
    case CSSAtRuleID::kCSSAtRuleSwash:
      return WebFeature::kCSSAtRuleSwash;
    case CSSAtRuleID::kCSSAtRuleSupports:
      return WebFeature::kCSSAtRuleSupports;
    case CSSAtRuleID::kCSSAtRulePositionFallback:
    case CSSAtRuleID::kCSSAtRuleTry:
      return WebFeature::kCSSAnchorPositioning;
    case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
      return WebFeature::kCSSAtRuleWebkitKeyframes;
    case CSSAtRuleID::kCSSAtRuleInvalid:
      NOTREACHED();
      return absl::nullopt;
  }
}

}  // namespace

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  if (absl::optional<WebFeature> feature = AtRuleFeature(rule_id)) {
    context->Count(*feature);
  }
}

}  // namespace blink
