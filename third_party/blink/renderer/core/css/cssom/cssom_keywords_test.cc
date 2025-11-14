// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cssom_keywords.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ComputedStyle;

namespace {

class CSSOMKeywordsTest : public PageTestBase {};

}  // namespace

TEST_F(CSSOMKeywordsTest, ApplyCSSKeywordValues) {
  Document& document = GetDocument();
  const ComputedStyle* initial =
      document.GetStyleResolver().InitialStyleForElement();

  StyleResolverState state(document, *document.documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial));
  state.CreateNewClonedStyle(*initial);

  for (int i = 1; i < kNumCSSValueKeywords; i++) {
    CSSValueID keyword = static_cast<CSSValueID>(i);
    if (css_parsing_utils::IsCSSWideKeyword(keyword)) {
      continue;
    }
    CSSKeywordValue* keyword_value =
        MakeGarbageCollected<CSSKeywordValue>(keyword);
    CSSIdentifierValue* ident_value = CSSIdentifierValue::Create(keyword);
    for (CSSPropertyID property_id : CSSPropertyIDList()) {
      switch (property_id) {
        // TODO(crbug.com/460361858): These properties need to support the
        // listed keywords in css_properties.json5 as CSSIdentifierValue
        // internally, or remove such keywords from the list of keywords.
        //
        // The presence of the properties below means there are existing CSS
        // Typed OM crash bugs for these properties.
        //
        // *** DO NOT ADD ADDITIONAL PROPERTIES BELOW ***
        case CSSPropertyID::kAnimationName:
        case CSSPropertyID::kAnimationFillMode:
        case CSSPropertyID::kAnimationTimeline:
        case CSSPropertyID::kTimelineTriggerSource:
        case CSSPropertyID::kTransitionProperty:
        case CSSPropertyID::kPositionArea:
        case CSSPropertyID::kAnchorScope:
        case CSSPropertyID::kAnimationDirection:
        case CSSPropertyID::kTransitionBehavior:
        case CSSPropertyID::kFontSizeAdjust:
        case CSSPropertyID::kContain:
        case CSSPropertyID::kTouchAction:
        case CSSPropertyID::kTextDecorationLine:
        case CSSPropertyID::kAnimationComposition:
        case CSSPropertyID::kAnimationIterationCount:
        case CSSPropertyID::kAnimationPlayState:
        case CSSPropertyID::kAnimationTimingFunction:
        case CSSPropertyID::kTransitionTimingFunction:
        case CSSPropertyID::kScrollSnapType:
        case CSSPropertyID::kContainerType:
        case CSSPropertyID::kFontVariantLigatures:
        case CSSPropertyID::kFontVariantEastAsian:
        case CSSPropertyID::kFontVariantNumeric:
        case CSSPropertyID::kBackgroundImage:
        case CSSPropertyID::kOffsetRotate:
        case CSSPropertyID::kGridAutoColumns:
        case CSSPropertyID::kGridAutoRows:
        case CSSPropertyID::kClipPath:
        case CSSPropertyID::kPositionTryFallbacks:
          continue;
        default:
          break;
      }
      if (!CSSOMKeywords::ValidKeywordForProperty(property_id,
                                                  *keyword_value)) {
        continue;
      }
      auto* longhand_property =
          DynamicTo<Longhand>(CSSProperty::Get(property_id));
      if (!longhand_property || !longhand_property->IsProperty() ||
          longhand_property->IsInternal() || longhand_property->IsSurrogate()) {
        continue;
      }
      longhand_property->ApplyValue(state, *ident_value,
                                    CSSProperty::ValueMode::kNormal);
    }
  }
}

}  // namespace blink
