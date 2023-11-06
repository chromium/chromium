// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"

namespace blink {

class AnimationInterpolableValueTest : public testing::Test {
 protected:
  double InterpolateNumbers(int a, int b, double progress) {
    // We require a property that maps to CSSNumberInterpolationType. 'z-index'
    // suffices for this, and also means we can ignore the AnimatableValues for
    // the compositor (as z-index isn't compositor-compatible).
    PropertyHandle property_handle(GetCSSPropertyZIndex());
    CSSNumberInterpolationType interpolation_type(property_handle);
    InterpolationValue start(MakeGarbageCollected<InterpolableNumber>(a));
    InterpolationValue end(MakeGarbageCollected<InterpolableNumber>(b));
    TransitionInterpolation* i = MakeGarbageCollected<TransitionInterpolation>(
        property_handle, interpolation_type, std::move(start), std::move(end),
        nullptr, nullptr);

    i->Interpolate(0, progress);
    TypedInterpolationValue* interpolated_value = i->GetInterpolatedValue();
    EXPECT_TRUE(interpolated_value);
    return To<InterpolableNumber>(interpolated_value->GetInterpolableValue())
        .Value();
  }

  void ScaleAndAdd(InterpolableValue& base,
                   double scale,
                   const InterpolableValue& add) {
    base.ScaleAndAdd(scale, add);
  }

  InterpolableValue* InterpolateLists(InterpolableValue* list_a,
                                      InterpolableValue* list_b,
                                      double progress) {
    InterpolableValue* result = list_a->CloneAndZero();
    list_a->Interpolate(*list_b, progress, *result);
    return result;
  }
};

TEST_F(AnimationInterpolableValueTest, InterpolateNumbers) {
  EXPECT_FLOAT_EQ(126, InterpolateNumbers(42, 0, -2));
  EXPECT_FLOAT_EQ(42, InterpolateNumbers(42, 0, 0));
  EXPECT_FLOAT_EQ(29.4f, InterpolateNumbers(42, 0, 0.3));
  EXPECT_FLOAT_EQ(21, InterpolateNumbers(42, 0, 0.5));
  EXPECT_FLOAT_EQ(0, InterpolateNumbers(42, 0, 1));
  EXPECT_FLOAT_EQ(-21, InterpolateNumbers(42, 0, 1.5));
}

TEST_F(AnimationInterpolableValueTest, SimpleList) {
  auto* list_a = MakeGarbageCollected<InterpolableList>(3);
  list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  list_a->Set(1, MakeGarbageCollected<InterpolableNumber>(42));
  list_a->Set(2, MakeGarbageCollected<InterpolableNumber>(20.5));

  auto* list_b = MakeGarbageCollected<InterpolableList>(3);
  list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  list_b->Set(1, MakeGarbageCollected<InterpolableNumber>(-200));
  list_b->Set(2, MakeGarbageCollected<InterpolableNumber>(300));

  InterpolableValue* interpolated_value =
      InterpolateLists(std::move(list_a), std::move(list_b), 0.3);
  const auto& out_list = To<InterpolableList>(*interpolated_value);

  EXPECT_FLOAT_EQ(30, To<InterpolableNumber>(out_list.Get(0))->Value());
  EXPECT_FLOAT_EQ(-30.6f, To<InterpolableNumber>(out_list.Get(1))->Value());
  EXPECT_FLOAT_EQ(104.35f, To<InterpolableNumber>(out_list.Get(2))->Value());
}

TEST_F(AnimationInterpolableValueTest, NestedList) {
  auto* list_a = MakeGarbageCollected<InterpolableList>(3);
  list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  auto* sub_list_a = MakeGarbageCollected<InterpolableList>(1);
  sub_list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  list_a->Set(1, sub_list_a);
  list_a->Set(2, MakeGarbageCollected<InterpolableNumber>(0));

  auto* list_b = MakeGarbageCollected<InterpolableList>(3);
  list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  auto* sub_list_b = MakeGarbageCollected<InterpolableList>(1);
  sub_list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(50));
  list_b->Set(1, sub_list_b);
  list_b->Set(2, MakeGarbageCollected<InterpolableNumber>(1));

  InterpolableValue* interpolated_value = InterpolateLists(list_a, list_b, 0.5);
  const auto& out_list = To<InterpolableList>(*interpolated_value);

  EXPECT_FLOAT_EQ(50, To<InterpolableNumber>(out_list.Get(0))->Value());
  EXPECT_FLOAT_EQ(
      75, To<InterpolableNumber>(To<InterpolableList>(out_list.Get(1))->Get(0))
              ->Value());
  EXPECT_FLOAT_EQ(0.5, To<InterpolableNumber>(out_list.Get(2))->Value());
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddNumbers) {
  InterpolableNumber* base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, 2, *MakeGarbageCollected<InterpolableNumber>(1));
  EXPECT_FLOAT_EQ(21, base->Value());

  base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, 0, *MakeGarbageCollected<InterpolableNumber>(5));
  EXPECT_FLOAT_EQ(5, base->Value());

  base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, -1, *MakeGarbageCollected<InterpolableNumber>(8));
  EXPECT_FLOAT_EQ(-2, base->Value());
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddLists) {
  auto* base_list = MakeGarbageCollected<InterpolableList>(3);
  base_list->Set(0, MakeGarbageCollected<InterpolableNumber>(5));
  base_list->Set(1, MakeGarbageCollected<InterpolableNumber>(10));
  base_list->Set(2, MakeGarbageCollected<InterpolableNumber>(15));
  auto* add_list = MakeGarbageCollected<InterpolableList>(3);
  add_list->Set(0, MakeGarbageCollected<InterpolableNumber>(1));
  add_list->Set(1, MakeGarbageCollected<InterpolableNumber>(2));
  add_list->Set(2, MakeGarbageCollected<InterpolableNumber>(3));
  ScaleAndAdd(*base_list, 2, *add_list);
  EXPECT_FLOAT_EQ(11, To<InterpolableNumber>(base_list->Get(0))->Value());
  EXPECT_FLOAT_EQ(22, To<InterpolableNumber>(base_list->Get(1))->Value());
  EXPECT_FLOAT_EQ(33, To<InterpolableNumber>(base_list->Get(2))->Value());
}

}  // namespace blink
