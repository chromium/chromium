// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Metadata for at-rules. Sorted by name for binary search.
struct AtRuleEntry {
  const char* name;
  CSSAtRuleID id;
  WebFeature feature;
};

// clang-format off
constexpr AtRuleEntry kAtRuleEntries[] = {
    {"-webkit-keyframes", CSSAtRuleID::kCSSAtRuleWebkitKeyframes, WebFeature::kCSSAtRuleWebkitKeyframes},
    {"annotation", CSSAtRuleID::kCSSAtRuleAnnotation, WebFeature::kCSSAtRuleAnnotation},
    {"bottom-center", CSSAtRuleID::kCSSAtRuleBottomCenter, WebFeature::kCSSAtRulePageMargin},
    {"bottom-left", CSSAtRuleID::kCSSAtRuleBottomLeft, WebFeature::kCSSAtRulePageMargin},
    {"bottom-left-corner", CSSAtRuleID::kCSSAtRuleBottomLeftCorner, WebFeature::kCSSAtRulePageMargin},
    {"bottom-right", CSSAtRuleID::kCSSAtRuleBottomRight, WebFeature::kCSSAtRulePageMargin},
    {"bottom-right-corner", CSSAtRuleID::kCSSAtRuleBottomRightCorner, WebFeature::kCSSAtRulePageMargin},
    {"character-variant", CSSAtRuleID::kCSSAtRuleCharacterVariant, WebFeature::kCSSAtRuleCharacterVariant},
    {"charset", CSSAtRuleID::kCSSAtRuleCharset, WebFeature::kCSSAtRuleCharset},
    {"container", CSSAtRuleID::kCSSAtRuleContainer, WebFeature::kCSSAtRuleContainer},
    {"counter-style", CSSAtRuleID::kCSSAtRuleCounterStyle, WebFeature::kCSSAtRuleCounterStyle},
    {"font-face", CSSAtRuleID::kCSSAtRuleFontFace, WebFeature::kCSSAtRuleFontFace},
    {"font-feature-values", CSSAtRuleID::kCSSAtRuleFontFeatureValues, WebFeature::kCSSAtRuleFontFeatureValues},
    {"font-palette-values", CSSAtRuleID::kCSSAtRuleFontPaletteValues, WebFeature::kCSSAtRuleFontPaletteValues},
    {"import", CSSAtRuleID::kCSSAtRuleImport, WebFeature::kCSSAtRuleImport},
    {"keyframes", CSSAtRuleID::kCSSAtRuleKeyframes, WebFeature::kCSSAtRuleKeyframes},
    {"layer", CSSAtRuleID::kCSSAtRuleLayer, WebFeature::kCSSCascadeLayers},
    {"left-bottom", CSSAtRuleID::kCSSAtRuleLeftBottom, WebFeature::kCSSAtRulePageMargin},
    {"left-middle", CSSAtRuleID::kCSSAtRuleLeftMiddle, WebFeature::kCSSAtRulePageMargin},
    {"left-top", CSSAtRuleID::kCSSAtRuleLeftTop, WebFeature::kCSSAtRulePageMargin},
    {"media", CSSAtRuleID::kCSSAtRuleMedia, WebFeature::kCSSAtRuleMedia},
    {"namespace", CSSAtRuleID::kCSSAtRuleNamespace, WebFeature::kCSSAtRuleNamespace},
    {"ornaments", CSSAtRuleID::kCSSAtRuleOrnaments, WebFeature::kCSSAtRuleOrnaments},
    {"page", CSSAtRuleID::kCSSAtRulePage, WebFeature::kCSSAtRulePage},
    {"position-try", CSSAtRuleID::kCSSAtRulePositionTry, WebFeature::kCSSAnchorPositioning},
    {"property", CSSAtRuleID::kCSSAtRuleProperty, WebFeature::kCSSAtRuleProperty},
    {"right-bottom", CSSAtRuleID::kCSSAtRuleRightBottom, WebFeature::kCSSAtRulePageMargin},
    {"right-middle", CSSAtRuleID::kCSSAtRuleRightMiddle, WebFeature::kCSSAtRulePageMargin},
    {"right-top", CSSAtRuleID::kCSSAtRuleRightTop, WebFeature::kCSSAtRulePageMargin},
    {"scope", CSSAtRuleID::kCSSAtRuleScope, WebFeature::kCSSAtRuleScope},
    {"starting-style", CSSAtRuleID::kCSSAtRuleStartingStyle, WebFeature::kCSSAtRuleStartingStyle},
    {"styleset", CSSAtRuleID::kCSSAtRuleStyleset, WebFeature::kCSSAtRuleStylistic},
    {"stylistic", CSSAtRuleID::kCSSAtRuleStylistic, WebFeature::kCSSAtRuleStylistic},
    {"supports", CSSAtRuleID::kCSSAtRuleSupports, WebFeature::kCSSAtRuleSupports},
    {"swash", CSSAtRuleID::kCSSAtRuleSwash, WebFeature::kCSSAtRuleSwash},
    {"top-center", CSSAtRuleID::kCSSAtRuleTopCenter, WebFeature::kCSSAtRulePageMargin},
    {"top-left", CSSAtRuleID::kCSSAtRuleTopLeft, WebFeature::kCSSAtRulePageMargin},
    {"top-left-corner", CSSAtRuleID::kCSSAtRuleTopLeftCorner, WebFeature::kCSSAtRulePageMargin},
    {"top-right", CSSAtRuleID::kCSSAtRuleTopRight, WebFeature::kCSSAtRulePageMargin},
    {"top-right-corner", CSSAtRuleID::kCSSAtRuleTopRightCorner, WebFeature::kCSSAtRulePageMargin},
    {"view-transition", CSSAtRuleID::kCSSAtRuleViewTransition, WebFeature::kCSSAtRuleViewTransition},
};
// clang-format on

// At-rules gated behind runtime flags.
// Sorted by name for consistency with kAtRuleEntries.
struct FlaggedAtRuleEntry {
  const char* name;
  CSSAtRuleID id;
  WebFeature feature;
  bool (*is_enabled)();
};

// clang-format off
constexpr FlaggedAtRuleEntry kFlaggedAtRuleEntries[] = {
    {"apply", CSSAtRuleID::kCSSAtRuleApplyMixin, WebFeature::kCSSMixins,
     &RuntimeEnabledFeatures::CSSMixinsEnabled},
    {"contents", CSSAtRuleID::kCSSAtRuleContents, WebFeature::kCSSMixins,
     &RuntimeEnabledFeatures::CSSMixinsEnabled},
    {"custom-media", CSSAtRuleID::kCSSAtRuleCustomMedia, WebFeature::kCSSCustomMedia,
     &RuntimeEnabledFeatures::CSSCustomMediaEnabled},
    {"function", CSSAtRuleID::kCSSAtRuleFunction, WebFeature::kCSSFunctions,
     &RuntimeEnabledFeatures::CSSFunctionsEnabled},
    {"mixin", CSSAtRuleID::kCSSAtRuleMixin, WebFeature::kCSSMixins,
     &RuntimeEnabledFeatures::CSSMixinsEnabled},
    {"navigation", CSSAtRuleID::kCSSAtRuleNavigation, WebFeature::kCSSAtRuleRoute,
     &RuntimeEnabledFeatures::RouteMatchingEnabled},
    {"result", CSSAtRuleID::kCSSAtRuleResult, WebFeature::kCSSMixins,
     &RuntimeEnabledFeatures::CSSMixinsEnabled},
    {"route", CSSAtRuleID::kCSSAtRuleRoute, WebFeature::kCSSAtRuleRoute,
     &RuntimeEnabledFeatures::RouteMatchingEnabled},
};
// clang-format on

// Compile-time validation that tables are sorted for binary search.
constexpr auto AtRuleNameProjection = [](const auto& entry) {
  return std::string_view(entry.name);
};

static_assert(std::ranges::is_sorted(kAtRuleEntries, {}, AtRuleNameProjection),
              "kAtRuleEntries must be sorted by name for binary search");
static_assert(std::ranges::is_sorted(kFlaggedAtRuleEntries,
                                     {},
                                     AtRuleNameProjection),
              "kFlaggedAtRuleEntries must be sorted by name");

}  // namespace

CSSAtRuleID CssAtRuleID(StringView name) {
  // Binary search the main table.
  const auto* it = std::lower_bound(
      std::begin(kAtRuleEntries), std::end(kAtRuleEntries), name,
      [](const AtRuleEntry& entry, const StringView& target) {
        return CodeUnitCompareIgnoringAsciiCase(StringView(entry.name),
                                                target) < 0;
      });
  if (it != std::end(kAtRuleEntries) &&
      EqualIgnoringAsciiCase(name, it->name)) {
    return it->id;
  }
  // Linear search the smaller flagged entries table.
  for (const auto& entry : kFlaggedAtRuleEntries) {
    if (entry.is_enabled() && EqualIgnoringAsciiCase(name, entry.name)) {
      return entry.id;
    }
  }
  return CSSAtRuleID::kCSSAtRuleInvalid;
}

StringView CssAtRuleIDToString(CSSAtRuleID id) {
  for (const auto& entry : kAtRuleEntries) {
    if (entry.id == id) {
      return entry.name;
    }
  }
  for (const auto& entry : kFlaggedAtRuleEntries) {
    if (entry.id == id) {
      return entry.name;
    }
  }
  NOTREACHED();
}

namespace {

std::optional<WebFeature> AtRuleFeature(CSSAtRuleID rule_id) {
  for (const auto& entry : kAtRuleEntries) {
    if (entry.id == rule_id) {
      return entry.feature;
    }
  }
  for (const auto& entry : kFlaggedAtRuleEntries) {
    if (entry.id == rule_id) {
      return entry.feature;
    }
  }
  NOTREACHED();
}

}  // namespace

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  if (std::optional<WebFeature> feature = AtRuleFeature(rule_id)) {
    context->Count(*feature);
  }
}

}  // namespace blink
