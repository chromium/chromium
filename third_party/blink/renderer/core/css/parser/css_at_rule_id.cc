// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

CSSAtRuleID CssAtRuleID(StringView name) {
  if (EqualIgnoringASCIICase(name, "charset"))
    return kCSSAtRuleCharset;
  if (EqualIgnoringASCIICase(name, "font-face"))
    return kCSSAtRuleFontFace;
  if (EqualIgnoringASCIICase(name, "import"))
    return kCSSAtRuleImport;
  if (EqualIgnoringASCIICase(name, "keyframes"))
    return kCSSAtRuleKeyframes;
  if (EqualIgnoringASCIICase(name, "layer")) {
    if (RuntimeEnabledFeatures::CSSCascadeLayersEnabled())
      return kCSSAtRuleLayer;
    return kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "media"))
    return kCSSAtRuleMedia;
  if (EqualIgnoringASCIICase(name, "namespace"))
    return kCSSAtRuleNamespace;
  if (EqualIgnoringASCIICase(name, "page"))
    return kCSSAtRulePage;
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
  if (EqualIgnoringASCIICase(name, "supports"))
    return kCSSAtRuleSupports;
  if (EqualIgnoringASCIICase(name, "viewport"))
    return kCSSAtRuleViewport;
  if (EqualIgnoringASCIICase(name, "-webkit-keyframes"))
    return kCSSAtRuleWebkitKeyframes;
  return kCSSAtRuleInvalid;
}

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  WebFeature feature;

  switch (rule_id) {
    case kCSSAtRuleCharset:
      feature = WebFeature::kCSSAtRuleCharset;
      break;
    case kCSSAtRuleFontFace:
      feature = WebFeature::kCSSAtRuleFontFace;
      break;
    case kCSSAtRuleImport:
      feature = WebFeature::kCSSAtRuleImport;
      break;
    case kCSSAtRuleKeyframes:
      feature = WebFeature::kCSSAtRuleKeyframes;
      break;
    case kCSSAtRuleLayer:
      feature = WebFeature::kCSSCascadeLayers;
      break;
    case kCSSAtRuleMedia:
      feature = WebFeature::kCSSAtRuleMedia;
      break;
    case kCSSAtRuleNamespace:
      feature = WebFeature::kCSSAtRuleNamespace;
      break;
    case kCSSAtRulePage:
      feature = WebFeature::kCSSAtRulePage;
      break;
    case kCSSAtRuleProperty:
      feature = WebFeature::kCSSAtRuleProperty;
      break;
    case kCSSAtRuleContainer:
      // TODO(crbug.com/1145970): Add use-counter.
      return;
    case kCSSAtRuleCounterStyle:
      feature = WebFeature::kCSSAtRuleCounterStyle;
      break;
    case kCSSAtRuleScrollTimeline:
      feature = WebFeature::kCSSAtRuleScrollTimeline;
      break;
    case kCSSAtRuleSupports:
      feature = WebFeature::kCSSAtRuleSupports;
      break;
    case kCSSAtRuleViewport:
      feature = WebFeature::kCSSAtRuleViewport;
      break;

    case kCSSAtRuleWebkitKeyframes:
      feature = WebFeature::kCSSAtRuleWebkitKeyframes;
      break;

    case kCSSAtRuleInvalid:
    // fallthrough
    default:
      NOTREACHED();
      return;
  }
  context->Count(feature);
}

}  // namespace blink
