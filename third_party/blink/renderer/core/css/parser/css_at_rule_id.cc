// Copyright 2015 The Chromium Authors. All rights reserved.
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
  if (EqualIgnoringASCIICase(name, "charset"))
    return kCSSAtRuleCharset;
  if (EqualIgnoringASCIICase(name, "font-face"))
    return kCSSAtRuleFontFace;
  if (EqualIgnoringASCIICase(name, "font-palette-values")) {
    if (RuntimeEnabledFeatures::FontPaletteEnabled())
      return kCSSAtRuleFontPaletteValues;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "import"))
    return kCSSAtRuleImport;
  if (EqualIgnoringASCIICase(name, "keyframes"))
    return kCSSAtRuleKeyframes;
  if (EqualIgnoringASCIICase(name, "layer")) {
    return kCSSAtRuleLayer;
  }
  if (EqualIgnoringASCIICase(name, "media"))
    return kCSSAtRuleMedia;
  if (EqualIgnoringASCIICase(name, "namespace"))
    return kCSSAtRuleNamespace;
  if (EqualIgnoringASCIICase(name, "page"))
    return kCSSAtRulePage;
  if (EqualIgnoringASCIICase(name, "position-fallback")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled())
      return kCSSAtRulePositionFallback;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "property"))
    return kCSSAtRuleProperty;
  if (EqualIgnoringASCIICase(name, "container")) {
    if (RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
      return kCSSAtRuleContainer;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "counter-style"))
    return kCSSAtRuleCounterStyle;
  if (EqualIgnoringASCIICase(name, "scroll-timeline")) {
    if (RuntimeEnabledFeatures::CSSScrollTimelineEnabled())
      return kCSSAtRuleScrollTimeline;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "scope")) {
    if (RuntimeEnabledFeatures::CSSScopeEnabled())
      return kCSSAtRuleScope;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "supports"))
    return kCSSAtRuleSupports;
  if (EqualIgnoringASCIICase(name, "try")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled())
      return kCSSAtRuleTry;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "viewport"))
    return kCSSAtRuleViewport;
  if (EqualIgnoringASCIICase(name, "-webkit-keyframes"))
    return kCSSAtRuleWebkitKeyframes;
  return kCSSAtRuleInvalid;
}

namespace {

absl::optional<WebFeature> AtRuleFeature(CSSAtRuleID rule_id) {
  switch (rule_id) {
    case kCSSAtRuleCharset:
      return WebFeature::kCSSAtRuleCharset;
    case kCSSAtRuleFontFace:
      return WebFeature::kCSSAtRuleFontFace;
    case kCSSAtRuleFontPaletteValues:
      return WebFeature::kCSSAtRuleFontPaletteValues;
    case kCSSAtRuleImport:
      return WebFeature::kCSSAtRuleImport;
    case kCSSAtRuleKeyframes:
      return WebFeature::kCSSAtRuleKeyframes;
    case kCSSAtRuleLayer:
      return WebFeature::kCSSCascadeLayers;
    case kCSSAtRuleMedia:
      return WebFeature::kCSSAtRuleMedia;
    case kCSSAtRuleNamespace:
      return WebFeature::kCSSAtRuleNamespace;
    case kCSSAtRulePage:
      return WebFeature::kCSSAtRulePage;
    case kCSSAtRuleProperty:
      return WebFeature::kCSSAtRuleProperty;
    case kCSSAtRuleContainer:
      return WebFeature::kCSSAtRuleContainer;
    case kCSSAtRuleCounterStyle:
      return WebFeature::kCSSAtRuleCounterStyle;
    case kCSSAtRuleScope:
      return WebFeature::kCSSAtRuleScope;
    case kCSSAtRuleScrollTimeline:
      return WebFeature::kCSSAtRuleScrollTimeline;
    case kCSSAtRuleSupports:
      return WebFeature::kCSSAtRuleSupports;
    case kCSSAtRuleViewport:
      return WebFeature::kCSSAtRuleViewport;
    case kCSSAtRulePositionFallback:
    case kCSSAtRuleTry:
      // TODO(crbug.com/1309178): Add use counter.
      return absl::nullopt;
    case kCSSAtRuleWebkitKeyframes:
      return WebFeature::kCSSAtRuleWebkitKeyframes;
    case kCSSAtRuleInvalid:
      [[fallthrough]];
    default:
      NOTREACHED();
      return absl::nullopt;
  }
}

}  // namespace

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  if (absl::optional<WebFeature> feature = AtRuleFeature(rule_id))
    context->Count(*feature);
}

}  // namespace blink
