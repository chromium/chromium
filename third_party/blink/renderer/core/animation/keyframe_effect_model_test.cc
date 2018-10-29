/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_double.h"
#include "third_party/blink/renderer/core/animation/animation_test_helper.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/property_descriptor.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class AnimationKeyframeEffectModel : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    element = GetDocument().CreateElementForBinding("foo");
  }

  void ExpectLengthValue(double expected_value,
                         Interpolation* interpolation_value) {
    ActiveInterpolations interpolations;
    interpolations.push_back(interpolation_value);
    EnsureInterpolatedValueCached(interpolations, GetDocument(), element);

    const TypedInterpolationValue* typed_value =
        ToInvalidatableInterpolation(interpolation_value)
            ->GetCachedValueForTesting();
    // Length values are stored as a list of values; here we assume pixels.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsList());
    const InterpolableList* list =
        ToInterpolableList(&typed_value->GetInterpolableValue());
    EXPECT_FLOAT_EQ(expected_value,
                    ToInterpolableNumber(list->Get(0))->Value());
  }

  void ExpectNonInterpolableValue(const String& expected_value,
                                  Interpolation* interpolation_value) {
    ActiveInterpolations interpolations;
    interpolations.push_back(interpolation_value);
    EnsureInterpolatedValueCached(interpolations, GetDocument(), element);

    const TypedInterpolationValue* typed_value =
        ToInvalidatableInterpolation(interpolation_value)
            ->GetCachedValueForTesting();
    const NonInterpolableValue* non_interpolable_value =
        typed_value->GetNonInterpolableValue();
    ASSERT_TRUE(IsCSSDefaultNonInterpolableValue(non_interpolable_value));

    const CSSValue* css_value =
        ToCSSDefaultNonInterpolableValue(non_interpolable_value)->CssValue();
    EXPECT_EQ(expected_value, css_value->CssText());
  }

  Persistent<Element> element;
};

const AnimationTimeDelta kDuration = AnimationTimeDelta::FromSecondsD(1);

StringKeyframeVector KeyframesAtZeroAndOne(CSSPropertyID property,
                                           const String& zero_value,
                                           const String& one_value) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      property, zero_value, SecureContextMode::kInsecureContext, nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(
      property, one_value, SecureContextMode::kInsecureContext, nullptr);
  return keyframes;
}

StringKeyframeVector KeyframesAtZeroAndOne(
    AtomicString property_name,
    const PropertyRegistry* property_registry,
    const String& zero_value,
    const String& one_value) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      property_name, property_registry, zero_value,
      SecureContextMode::kInsecureContext, nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(property_name, property_registry, one_value,
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  return keyframes;
}

void ExpectProperty(CSSPropertyID property,
                    Interpolation* interpolation_value) {
  InvalidatableInterpolation* interpolation =
      ToInvalidatableInterpolation(interpolation_value);
  const PropertyHandle& property_handle = interpolation->GetProperty();
  ASSERT_TRUE(property_handle.IsCSSProperty());
  ASSERT_EQ(property, property_handle.GetCSSProperty().PropertyID());
}

Interpolation* FindValue(HeapVector<Member<Interpolation>>& values,
                         CSSPropertyID id) {
  for (auto& value : values) {
    const PropertyHandle& property =
        ToInvalidatableInterpolation(value)->GetProperty();
    if (property.IsCSSProperty() &&
        property.GetCSSProperty().PropertyID() == id)
      return value;
  }
  return nullptr;
}

TEST_F(AnimationKeyframeEffectModel, BasicOperation) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyFontFamily, "serif", "cursive");
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ASSERT_EQ(1UL, values.size());
  ExpectProperty(CSSPropertyFontFamily, values.at(0));
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeReplaceNonInterpolable) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyFontFamily, "serif", "cursive");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeReplace) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_CompositeAdd) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeEaseIn) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.8579516, values.at(0));
  effect->Sample(0, 0.6, kDuration * 100, values);
  ExpectLengthValue(3.8582394, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeCubicBezier) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Create(0.42, 0, 0.58, 1));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(4.3363357, values.at(0));
  effect->Sample(0, 0.6, kDuration * 1000, values);
  ExpectLengthValue(4.3362322, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ExtrapolateReplaceNonInterpolable) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyFontFamily, "serif", "cursive");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ExtrapolateReplace) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectLengthValue(3.0 * -0.6 + 5.0 * 1.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_ExtrapolateAdd) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * -0.6 + (7.0 + 5.0) * 1.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ZeroKeyframes) {
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(StringKeyframeVector());
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.5, kDuration, values);
  EXPECT_TRUE(values.IsEmpty());
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetZero) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetOne) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(1.0);
  keyframes[0]->SetCSSPropertyValue(
      CSSPropertyLeft, "5px", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(7.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, MoreThanTwoKeyframes) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = StringKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.3, kDuration, values);
  ExpectNonInterpolableValue("sans-serif", values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, EndKeyframeOffsetsUnspecified) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = StringKeyframe::Create();
  keyframes[2]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.1, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
  effect->Sample(0, 0.9, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, SampleOnKeyframe) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = StringKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.0, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
  effect->Sample(0, 1.0, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, MultipleKeyframesWithSameOffset) {
  StringKeyframeVector keyframes(9);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(0.1);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = StringKeyframe::Create();
  keyframes[2]->SetOffset(0.1);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyFontFamily, "monospace",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[3] = StringKeyframe::Create();
  keyframes[3]->SetOffset(0.5);
  keyframes[3]->SetCSSPropertyValue(CSSPropertyFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[4] = StringKeyframe::Create();
  keyframes[4]->SetOffset(0.5);
  keyframes[4]->SetCSSPropertyValue(CSSPropertyFontFamily, "fantasy",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[5] = StringKeyframe::Create();
  keyframes[5]->SetOffset(0.5);
  keyframes[5]->SetCSSPropertyValue(CSSPropertyFontFamily, "system-ui",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[6] = StringKeyframe::Create();
  keyframes[6]->SetOffset(0.9);
  keyframes[6]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[7] = StringKeyframe::Create();
  keyframes[7]->SetOffset(0.9);
  keyframes[7]->SetCSSPropertyValue(CSSPropertyFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[8] = StringKeyframe::Create();
  keyframes[8]->SetOffset(1.0);
  keyframes[8]->SetCSSPropertyValue(CSSPropertyFontFamily, "monospace",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.0, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
  effect->Sample(0, 0.2, kDuration, values);
  ExpectNonInterpolableValue("monospace", values.at(0));
  effect->Sample(0, 0.4, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  ExpectNonInterpolableValue("system-ui", values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("system-ui", values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
  effect->Sample(0, 1.0, kDuration, values);
  ExpectNonInterpolableValue("monospace", values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_PerKeyframeComposite) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      CSSPropertyLeft, "3px", SecureContextMode::kInsecureContext, nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(
      CSSPropertyLeft, "5px", SecureContextMode::kInsecureContext, nullptr);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.0 * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, MultipleProperties) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyFontStyle, "normal",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyFontStyle, "oblique",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  EXPECT_EQ(2UL, values.size());
  Interpolation* left_value = FindValue(values, CSSPropertyFontFamily);
  ASSERT_TRUE(left_value);
  ExpectNonInterpolableValue("cursive", left_value);
  Interpolation* right_value = FindValue(values, CSSPropertyFontStyle);
  ASSERT_TRUE(right_value);
  ExpectNonInterpolableValue("oblique", right_value);
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_RecompositeCompositableValue) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
  ExpectLengthValue((9.0 + 3.0) * 0.4 + (9.0 + 5.0) * 0.6, values.at(1));
}

TEST_F(AnimationKeyframeEffectModel, MultipleIterations) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyLeft, "1px", "3px");
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.5, kDuration, values);
  ExpectLengthValue(2.0, values.at(0));
  effect->Sample(1, 0.5, kDuration, values);
  ExpectLengthValue(2.0, values.at(0));
  effect->Sample(2, 0.5, kDuration, values);
  ExpectLengthValue(2.0, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_DependsOnUnderlyingValue) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      CSSPropertyLeft, "1px", SecureContextMode::kInsecureContext, nullptr);
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1] = StringKeyframe::Create();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(
      CSSPropertyLeft, "1px", SecureContextMode::kInsecureContext, nullptr);
  keyframes[2] = StringKeyframe::Create();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(
      CSSPropertyLeft, "1px", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.1, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.25, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.4, kDuration, values);
  EXPECT_TRUE(values.at(0));
  effect->Sample(0, 0.5, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.6, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.75, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  EXPECT_FALSE(values.at(0));
  effect->Sample(0, 1, kDuration, values);
  EXPECT_FALSE(values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, AddSyntheticKeyframes) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.5);
  keyframes[0]->SetCSSPropertyValue(
      CSSPropertyLeft, "4px", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);
  const StringPropertySpecificKeyframeVector& property_specific_keyframes =
      effect->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyLeft()));
  EXPECT_EQ(3U, property_specific_keyframes.size());
  EXPECT_DOUBLE_EQ(0.0, property_specific_keyframes[0]->Offset());
  EXPECT_DOUBLE_EQ(0.5, property_specific_keyframes[1]->Offset());
  EXPECT_DOUBLE_EQ(1.0, property_specific_keyframes[2]->Offset());
}

TEST_F(AnimationKeyframeEffectModel, ToKeyframeEffectModel) {
  StringKeyframeVector keyframes(0);
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);

  EffectModel* base_effect = effect;
  EXPECT_TRUE(ToStringKeyframeEffectModel(base_effect));
}

TEST_F(AnimationKeyframeEffectModel, CompositorSnapshotUpdateBasic) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyOpacity, "0", "1");
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);

  auto style = GetDocument().EnsureStyleResolver().StyleForElement(element);

  const AnimatableValue* value;

  // Animatable value should be empty before snapshot
  value = effect
              ->GetPropertySpecificKeyframes(
                  PropertyHandle(GetCSSPropertyOpacity()))[0]
              ->GetAnimatableValue();
  EXPECT_FALSE(value);

  // Snapshot should update first time after construction
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));
  // Snapshot should not update on second call
  EXPECT_FALSE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));
  // Snapshot should update after an explicit invalidation
  effect->InvalidateCompositorKeyframesSnapshot();
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));

  // Animatable value should be available after snapshot
  value = effect
              ->GetPropertySpecificKeyframes(
                  PropertyHandle(GetCSSPropertyOpacity()))[0]
              ->GetAnimatableValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());
}

TEST_F(AnimationKeyframeEffectModel,
       CompositorSnapshotUpdateAfterKeyframeChange) {
  StringKeyframeVector opacity_keyframes =
      KeyframesAtZeroAndOne(CSSPropertyOpacity, "0", "1");
  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(opacity_keyframes);

  auto style = GetDocument().EnsureStyleResolver().StyleForElement(element);

  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));

  const AnimatableValue* value;
  value = effect
              ->GetPropertySpecificKeyframes(
                  PropertyHandle(GetCSSPropertyOpacity()))[0]
              ->GetAnimatableValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());

  StringKeyframeVector filter_keyframes =
      KeyframesAtZeroAndOne(CSSPropertyFilter, "blur(1px)", "blur(10px)");
  effect->SetFrames(filter_keyframes);

  // Snapshot should update after changing keyframes
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));
  value = effect
              ->GetPropertySpecificKeyframes(
                  PropertyHandle(GetCSSPropertyFilter()))[0]
              ->GetAnimatableValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsFilterOperations());
}

TEST_F(AnimationKeyframeEffectModel, CompositorSnapshotUpdateCustomProperty) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  DummyExceptionStateForTesting exception_state;
  PropertyDescriptor property_descriptor;
  property_descriptor.setName("--foo");
  property_descriptor.setSyntax("<number>");
  property_descriptor.setInitialValue("0");
  property_descriptor.setInherits(false);
  PropertyRegistration::registerProperty(&GetDocument(), property_descriptor,
                                         exception_state);
  EXPECT_FALSE(exception_state.HadException());

  StringKeyframeVector keyframes = KeyframesAtZeroAndOne(
      AtomicString("--foo"), GetDocument().GetPropertyRegistry(), "0", "100");

  element->style()->setProperty(&GetDocument(), "--foo", "0", g_empty_string,
                                exception_state);
  EXPECT_FALSE(exception_state.HadException());

  StringKeyframeEffectModel* effect =
      StringKeyframeEffectModel::Create(keyframes);

  auto style = GetDocument().EnsureStyleResolver().StyleForElement(element);

  const AnimatableValue* value;

  // Snapshot should update first time after construction
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));

  // Animatable value should be available after snapshot
  value = effect
              ->GetPropertySpecificKeyframes(
                  PropertyHandle(AtomicString("--foo")))[0]
              ->GetAnimatableValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());
}

}  // namespace blink

namespace blink {

class KeyframeEffectModelTest : public testing::Test {
 public:
  static Vector<double> GetComputedOffsets(const KeyframeVector& keyframes) {
    return KeyframeEffectModelBase::GetComputedOffsets(keyframes);
  }
};

TEST_F(KeyframeEffectModelTest, EvenlyDistributed1) {
  KeyframeVector keyframes(5);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0.125);
  keyframes[1] = StringKeyframe::Create();
  keyframes[2] = StringKeyframe::Create();
  keyframes[3] = StringKeyframe::Create();
  keyframes[4] = StringKeyframe::Create();
  keyframes[4]->SetOffset(0.625);

  const Vector<double> result = GetComputedOffsets(keyframes);
  EXPECT_EQ(5U, result.size());
  EXPECT_DOUBLE_EQ(0.125, result[0]);
  EXPECT_DOUBLE_EQ(0.25, result[1]);
  EXPECT_DOUBLE_EQ(0.375, result[2]);
  EXPECT_DOUBLE_EQ(0.5, result[3]);
  EXPECT_DOUBLE_EQ(0.625, result[4]);
}

TEST_F(KeyframeEffectModelTest, EvenlyDistributed2) {
  KeyframeVector keyframes(6);
  keyframes[0] = StringKeyframe::Create();
  keyframes[1] = StringKeyframe::Create();
  keyframes[2] = StringKeyframe::Create();
  keyframes[3] = StringKeyframe::Create();
  keyframes[3]->SetOffset(0.75);
  keyframes[4] = StringKeyframe::Create();
  keyframes[5] = StringKeyframe::Create();

  const Vector<double> result = GetComputedOffsets(keyframes);
  EXPECT_EQ(6U, result.size());
  EXPECT_DOUBLE_EQ(0.0, result[0]);
  EXPECT_DOUBLE_EQ(0.25, result[1]);
  EXPECT_DOUBLE_EQ(0.5, result[2]);
  EXPECT_DOUBLE_EQ(0.75, result[3]);
  EXPECT_DOUBLE_EQ(0.875, result[4]);
  EXPECT_DOUBLE_EQ(1.0, result[5]);
}

TEST_F(KeyframeEffectModelTest, EvenlyDistributed3) {
  KeyframeVector keyframes(12);
  keyframes[0] = StringKeyframe::Create();
  keyframes[0]->SetOffset(0);
  keyframes[1] = StringKeyframe::Create();
  keyframes[2] = StringKeyframe::Create();
  keyframes[3] = StringKeyframe::Create();
  keyframes[4] = StringKeyframe::Create();
  keyframes[4]->SetOffset(0.5);
  keyframes[5] = StringKeyframe::Create();
  keyframes[6] = StringKeyframe::Create();
  keyframes[7] = StringKeyframe::Create();
  keyframes[7]->SetOffset(0.8);
  keyframes[8] = StringKeyframe::Create();
  keyframes[9] = StringKeyframe::Create();
  keyframes[10] = StringKeyframe::Create();
  keyframes[11] = StringKeyframe::Create();

  const Vector<double> result = GetComputedOffsets(keyframes);
  EXPECT_EQ(12U, result.size());
  EXPECT_DOUBLE_EQ(0.0, result[0]);
  EXPECT_DOUBLE_EQ(0.125, result[1]);
  EXPECT_DOUBLE_EQ(0.25, result[2]);
  EXPECT_DOUBLE_EQ(0.375, result[3]);
  EXPECT_DOUBLE_EQ(0.5, result[4]);
  EXPECT_DOUBLE_EQ(0.6, result[5]);
  EXPECT_DOUBLE_EQ(0.7, result[6]);
  EXPECT_DOUBLE_EQ(0.8, result[7]);
  EXPECT_DOUBLE_EQ(0.85, result[8]);
  EXPECT_DOUBLE_EQ(0.9, result[9]);
  EXPECT_DOUBLE_EQ(0.95, result[10]);
  EXPECT_DOUBLE_EQ(1.0, result[11]);
}

}  // namespace blink
