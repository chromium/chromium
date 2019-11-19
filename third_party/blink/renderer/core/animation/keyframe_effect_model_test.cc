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
#include "third_party/blink/renderer/core/animation/animation_test_helper.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_color.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class AnimationKeyframeEffectModel : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    GetDocument().UpdateStyleAndLayoutTree();
    element = GetDocument().CreateElementForBinding("foo");
    GetDocument().body()->appendChild(element);
  }

  void ExpectLengthValue(double expected_value,
                         Interpolation* interpolation_value) {
    ActiveInterpolations interpolations;
    interpolations.push_back(interpolation_value);
    EnsureInterpolatedValueCached(interpolations, GetDocument(), element);

    const TypedInterpolationValue* typed_value =
        ToInvalidatableInterpolation(interpolation_value)
            ->GetCachedValueForTesting();
    // Length values are stored as an |InterpolableLength|; here we assume
    // pixels.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsLength());
    const InterpolableLength& length =
        To<InterpolableLength>(typed_value->GetInterpolableValue());
    // Lengths are computed in logical units, which are quantized to 64ths of
    // a pixel.
    EXPECT_NEAR(expected_value,
                length.CreateCSSValue(kValueRangeAll)->GetDoubleValue(),
                /*abs_error=*/0.02);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      property, zero_value, SecureContextMode::kInsecureContext, nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(
      property, one_value, SecureContextMode::kInsecureContext, nullptr);
  return keyframes;
}

StringKeyframeVector KeyframesAtZeroAndOne(
    AtomicString property_name,
    const String& zero_value,
    const String& one_value) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(
      property_name, zero_value, SecureContextMode::kInsecureContext, nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(
      property_name, one_value, SecureContextMode::kInsecureContext, nullptr);
  return keyframes;
}

const PropertySpecificKeyframeVector& ConstructEffectAndGetKeyframes(
    const AtomicString& property_name,
    const AtomicString& type,
    Document* document,
    Element* element,
    const String& zero_value,
    const String& one_value,
    ExceptionState& exception_state) {
  css_test_helpers::RegisterProperty(*document, property_name, type, zero_value,
                                     false);

  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(property_name, zero_value, one_value);

  element->style()->setProperty(document, property_name, zero_value,
                                g_empty_string, exception_state);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  auto style = document->EnsureStyleResolver().StyleForElement(element);

  // Snapshot should update first time after construction
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));

  return *effect->GetPropertySpecificKeyframes(PropertyHandle(property_name));
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
      KeyframesAtZeroAndOne(CSSPropertyID::kFontFamily, "serif", "cursive");
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ASSERT_EQ(1UL, values.size());
  ExpectProperty(CSSPropertyID::kFontFamily, values.at(0));
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeReplaceNonInterpolable) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kFontFamily, "serif", "cursive");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeReplace) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_CompositeAdd) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeEaseIn) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE_IN));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  // CubicBezier(0.42, 0, 1, 1)(0.6) = 0.4291197695757142.
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.85824, values.at(0));
  effect->Sample(0, 0.6, kDuration * 100, values);
  ExpectLengthValue(3.85824, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, CompositeCubicBezier) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[0]->SetEasing(CubicBezierTimingFunction::Create(0.42, 0, 0.58, 1));
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  // CubicBezier(0.42, 0, 0.58, 1)(0.6) = 0.6681161300485039.
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(4.336232, values.at(0));
  effect->Sample(0, 0.6, kDuration * 1000, values);
  ExpectLengthValue(4.336232, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ExtrapolateReplaceNonInterpolable) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kFontFamily, "serif", "cursive");
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ExtrapolateReplace) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  keyframes[0]->SetComposite(EffectModel::kCompositeReplace);
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectLengthValue(3.0 * -0.6 + 5.0 * 1.6, values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_ExtrapolateAdd) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 1.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * -0.6 + (7.0 + 5.0) * 1.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, ZeroKeyframes) {
  auto* effect =
      MakeGarbageCollected<StringKeyframeEffectModel>(StringKeyframeVector());
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.5, kDuration, values);
  EXPECT_TRUE(values.IsEmpty());
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetZero) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectNonInterpolableValue("serif", values.at(0));
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_SingleKeyframeAtOffsetOne) {
  StringKeyframeVector keyframes(1);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(1.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kLeft, "5px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(7.0 * 0.4 + 5.0 * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, MoreThanTwoKeyframes) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.3, kDuration, values);
  ExpectNonInterpolableValue("sans-serif", values.at(0));
  effect->Sample(0, 0.8, kDuration, values);
  ExpectNonInterpolableValue("cursive", values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, EndKeyframeOffsetsUnspecified) {
  StringKeyframeVector keyframes(3);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(0.1);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2]->SetOffset(0.1);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "monospace",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[3] = MakeGarbageCollected<StringKeyframe>();
  keyframes[3]->SetOffset(0.5);
  keyframes[3]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[4] = MakeGarbageCollected<StringKeyframe>();
  keyframes[4]->SetOffset(0.5);
  keyframes[4]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "fantasy",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[5] = MakeGarbageCollected<StringKeyframe>();
  keyframes[5]->SetOffset(0.5);
  keyframes[5]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "system-ui",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[6] = MakeGarbageCollected<StringKeyframe>();
  keyframes[6]->SetOffset(0.9);
  keyframes[6]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[7] = MakeGarbageCollected<StringKeyframe>();
  keyframes[7]->SetOffset(0.9);
  keyframes[7]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "sans-serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[8] = MakeGarbageCollected<StringKeyframe>();
  keyframes[8]->SetOffset(1.0);
  keyframes[8]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "monospace",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kLeft, "3px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kLeft, "5px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue(3.0 * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
}

TEST_F(AnimationKeyframeEffectModel, MultipleProperties) {
  StringKeyframeVector keyframes(2);
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "serif",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kFontStyle, "normal",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontFamily, "cursive",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kFontStyle, "oblique",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  EXPECT_EQ(2UL, values.size());
  Interpolation* left_value = FindValue(values, CSSPropertyID::kFontFamily);
  ASSERT_TRUE(left_value);
  ExpectNonInterpolableValue("cursive", left_value);
  Interpolation* right_value = FindValue(values, CSSPropertyID::kFontStyle);
  ASSERT_TRUE(right_value);
  ExpectNonInterpolableValue("oblique", right_value);
}

// FIXME: Re-enable this test once compositing of CompositeAdd is supported.
TEST_F(AnimationKeyframeEffectModel, DISABLED_RecompositeCompositableValue) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "3px", "5px");
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1]->SetComposite(EffectModel::kCompositeAdd);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  HeapVector<Member<Interpolation>> values;
  effect->Sample(0, 0.6, kDuration, values);
  ExpectLengthValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values.at(0));
  ExpectLengthValue((9.0 + 3.0) * 0.4 + (9.0 + 5.0) * 0.6, values.at(1));
}

TEST_F(AnimationKeyframeEffectModel, MultipleIterations) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kLeft, "1px", "3px");
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kLeft, "1px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[0]->SetComposite(EffectModel::kCompositeAdd);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1]->SetOffset(0.5);
  keyframes[1]->SetCSSPropertyValue(CSSPropertyID::kLeft, "1px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2]->SetOffset(1.0);
  keyframes[2]->SetCSSPropertyValue(CSSPropertyID::kLeft, "1px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.5);
  keyframes[0]->SetCSSPropertyValue(CSSPropertyID::kLeft, "4px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  const StringPropertySpecificKeyframeVector& property_specific_keyframes =
      *effect->GetPropertySpecificKeyframes(
          PropertyHandle(GetCSSPropertyLeft()));
  EXPECT_EQ(3U, property_specific_keyframes.size());
  EXPECT_DOUBLE_EQ(0.0, property_specific_keyframes[0]->Offset());
  EXPECT_DOUBLE_EQ(0.5, property_specific_keyframes[1]->Offset());
  EXPECT_DOUBLE_EQ(1.0, property_specific_keyframes[2]->Offset());
}

TEST_F(AnimationKeyframeEffectModel, ToKeyframeEffectModel) {
  StringKeyframeVector keyframes(0);
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  EffectModel* base_effect = effect;
  EXPECT_TRUE(ToStringKeyframeEffectModel(base_effect));
}

TEST_F(AnimationKeyframeEffectModel, CompositorSnapshotUpdateBasic) {
  StringKeyframeVector keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kOpacity, "0", "1");
  auto* effect = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  auto style = GetDocument().EnsureStyleResolver().StyleForElement(element);

  const CompositorKeyframeValue* value;

  // Compositor keyframe value should be empty before snapshot
  const auto& empty_keyframes = *effect->GetPropertySpecificKeyframes(
      PropertyHandle(GetCSSPropertyOpacity()));
  value = empty_keyframes[0]->GetCompositorKeyframeValue();
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

  // Compositor keyframe value should be available after snapshot
  const auto& available_keyframes = *effect->GetPropertySpecificKeyframes(
      PropertyHandle(GetCSSPropertyOpacity()));
  value = available_keyframes[0]->GetCompositorKeyframeValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());
}

TEST_F(AnimationKeyframeEffectModel,
       CompositorSnapshotUpdateAfterKeyframeChange) {
  StringKeyframeVector opacity_keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kOpacity, "0", "1");
  auto* effect =
      MakeGarbageCollected<StringKeyframeEffectModel>(opacity_keyframes);

  auto style = GetDocument().EnsureStyleResolver().StyleForElement(element);

  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));

  const CompositorKeyframeValue* value;
  const auto& keyframes = *effect->GetPropertySpecificKeyframes(
      PropertyHandle(GetCSSPropertyOpacity()));
  value = keyframes[0]->GetCompositorKeyframeValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());

  StringKeyframeVector filter_keyframes =
      KeyframesAtZeroAndOne(CSSPropertyID::kFilter, "blur(1px)", "blur(10px)");
  effect->SetFrames(filter_keyframes);

  // Snapshot should update after changing keyframes
  EXPECT_TRUE(effect->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *style, nullptr));
  const auto& updated_keyframes = *effect->GetPropertySpecificKeyframes(
      PropertyHandle(GetCSSPropertyFilter()));
  value = updated_keyframes[0]->GetCompositorKeyframeValue();
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsFilterOperations());
}

TEST_F(AnimationKeyframeEffectModel, CompositorSnapshotUpdateCustomProperty) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  DummyExceptionStateForTesting exception_state;

  // Compositor keyframe value available after snapshot
  const CompositorKeyframeValue* value =
      ConstructEffectAndGetKeyframes("--foo", "<number>", &GetDocument(),
                                     element, "0", "100", exception_state)[1]
          ->GetCompositorKeyframeValue();
  ASSERT_FALSE(exception_state.HadException());

  // Test value holds the correct number type
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->IsDouble());
  EXPECT_EQ(ToCompositorKeyframeDouble(value)->ToDouble(), 100);
}

TEST_F(AnimationKeyframeEffectModel, CompositorUpdateColorProperty) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  DummyExceptionStateForTesting exception_state;

  element->style()->setProperty(&GetDocument(), "color", "rgb(0, 255, 0)",
                                g_empty_string, exception_state);

  // Compositor keyframe value available after snapshot
  const CompositorKeyframeValue* value_rgb =
      ConstructEffectAndGetKeyframes("--rgb", "<color>", &GetDocument(),
                                     element, "rgb(0, 0, 0)", "rgb(0, 255, 0)",
                                     exception_state)[1]
          ->GetCompositorKeyframeValue();
  const CompositorKeyframeValue* value_hsl =
      ConstructEffectAndGetKeyframes("--hsl", "<color>", &GetDocument(),
                                     element, "hsl(0, 0%, 0%)",
                                     "hsl(120, 100%, 50%)", exception_state)[1]
          ->GetCompositorKeyframeValue();
  const CompositorKeyframeValue* value_name =
      ConstructEffectAndGetKeyframes("--named", "<color>", &GetDocument(),
                                     element, "black", "lime",
                                     exception_state)[1]
          ->GetCompositorKeyframeValue();
  const CompositorKeyframeValue* value_hex =
      ConstructEffectAndGetKeyframes("--hex", "<color>", &GetDocument(),
                                     element, "#000000", "#00FF00",
                                     exception_state)[1]
          ->GetCompositorKeyframeValue();
  const CompositorKeyframeValue* value_curr =
      ConstructEffectAndGetKeyframes("--curr", "<color>", &GetDocument(),
                                     element, "#000000", "currentcolor",
                                     exception_state)[1]
          ->GetCompositorKeyframeValue();
  const PropertySpecificKeyframeVector& values_mixed =
      ConstructEffectAndGetKeyframes("--mixed", "<color>", &GetDocument(),
                                     element, "#000000", "lime",
                                     exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Test rgb color input
  EXPECT_TRUE(value_rgb);
  EXPECT_TRUE(value_rgb->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_rgb)->ToColor(), SK_ColorGREEN);

  // Test hsl color input
  EXPECT_TRUE(value_hsl);
  EXPECT_TRUE(value_hsl->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_hsl)->ToColor(), SK_ColorGREEN);

  // Test named color input
  EXPECT_TRUE(value_name);
  EXPECT_TRUE(value_name->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_name)->ToColor(), SK_ColorGREEN);

  // Test hex color input
  EXPECT_TRUE(value_hex);
  EXPECT_TRUE(value_hex->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_hex)->ToColor(), SK_ColorGREEN);

  // currentcolor is a CSSIdentifierValue not a color
  EXPECT_FALSE(value_curr);

  // Ensure both frames are consistent when values are mixed
  const CompositorKeyframeValue* value_mixed0 =
      values_mixed[0]->GetCompositorKeyframeValue();
  const CompositorKeyframeValue* value_mixed1 =
      values_mixed[1]->GetCompositorKeyframeValue();

  EXPECT_TRUE(value_mixed0);
  EXPECT_TRUE(value_mixed0->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_mixed0)->ToColor(), SK_ColorBLACK);

  EXPECT_TRUE(value_mixed1);
  EXPECT_TRUE(value_mixed1->IsColor());
  EXPECT_EQ(ToCompositorKeyframeColor(value_mixed1)->ToColor(), SK_ColorGREEN);
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0.125);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[3] = MakeGarbageCollected<StringKeyframe>();
  keyframes[4] = MakeGarbageCollected<StringKeyframe>();
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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[3] = MakeGarbageCollected<StringKeyframe>();
  keyframes[3]->SetOffset(0.75);
  keyframes[4] = MakeGarbageCollected<StringKeyframe>();
  keyframes[5] = MakeGarbageCollected<StringKeyframe>();

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
  keyframes[0] = MakeGarbageCollected<StringKeyframe>();
  keyframes[0]->SetOffset(0);
  keyframes[1] = MakeGarbageCollected<StringKeyframe>();
  keyframes[2] = MakeGarbageCollected<StringKeyframe>();
  keyframes[3] = MakeGarbageCollected<StringKeyframe>();
  keyframes[4] = MakeGarbageCollected<StringKeyframe>();
  keyframes[4]->SetOffset(0.5);
  keyframes[5] = MakeGarbageCollected<StringKeyframe>();
  keyframes[6] = MakeGarbageCollected<StringKeyframe>();
  keyframes[7] = MakeGarbageCollected<StringKeyframe>();
  keyframes[7]->SetOffset(0.8);
  keyframes[8] = MakeGarbageCollected<StringKeyframe>();
  keyframes[9] = MakeGarbageCollected<StringKeyframe>();
  keyframes[10] = MakeGarbageCollected<StringKeyframe>();
  keyframes[11] = MakeGarbageCollected<StringKeyframe>();

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
