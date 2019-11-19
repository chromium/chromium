/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/style/filter_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

TEST(FilterOperationsTest, mapRectNoFilter) {
  FilterOperations ops;
  EXPECT_FALSE(ops.HasFilterThatMovesPixels());
  EXPECT_EQ(FloatRect(0, 0, 10, 10), ops.MapRect(FloatRect(0, 0, 10, 10)));
}

TEST(FilterOperationsTest, mapRectBlur) {
  FilterOperations ops;
  ops.Operations().push_back(
      MakeGarbageCollected<BlurFilterOperation>(Length::Fixed(20.0)));
  EXPECT_TRUE(ops.HasFilterThatMovesPixels());
  EXPECT_EQ(IntRect(-57, -57, 124, 124),
            EnclosingIntRect(ops.MapRect(FloatRect(0, 0, 10, 10))));
}

TEST(FilterOperationsTest, mapRectDropShadow) {
  FilterOperations ops;
  ops.Operations().push_back(DropShadowFilterOperation::Create(ShadowData(
      FloatPoint(3, 8), 20, 0, kNormal, StyleColor(Color(1, 2, 3)))));
  EXPECT_TRUE(ops.HasFilterThatMovesPixels());
  EXPECT_EQ(IntRect(-54, -49, 124, 124),
            EnclosingIntRect(ops.MapRect(FloatRect(0, 0, 10, 10))));
}

TEST(FilterOperationsTest, mapRectBoxReflect) {
  FilterOperations ops;
  ops.Operations().push_back(MakeGarbageCollected<BoxReflectFilterOperation>(
      BoxReflection(BoxReflection::kVerticalReflection, 100)));
  EXPECT_TRUE(ops.HasFilterThatMovesPixels());

  // original IntRect(0, 0, 10, 10) + reflection IntRect(90, 90, 10, 10)
  EXPECT_EQ(FloatRect(0, 0, 10, 100), ops.MapRect(FloatRect(0, 0, 10, 10)));
}

TEST(FilterOperationsTest, mapRectDropShadowAndBoxReflect) {
  // This is a case where the order of filter operations matters, and it's
  // important that the bounds be filtered in the correct order.
  FilterOperations ops;
  ops.Operations().push_back(DropShadowFilterOperation::Create(ShadowData(
      FloatPoint(100, 200), 0, 0, kNormal, StyleColor(Color::kBlack))));
  ops.Operations().push_back(MakeGarbageCollected<BoxReflectFilterOperation>(
      BoxReflection(BoxReflection::kVerticalReflection, 50)));
  EXPECT_TRUE(ops.HasFilterThatMovesPixels());
  EXPECT_EQ(FloatRect(0, -160, 110, 370), ops.MapRect(FloatRect(0, 0, 10, 10)));
}

}  // namespace blink
