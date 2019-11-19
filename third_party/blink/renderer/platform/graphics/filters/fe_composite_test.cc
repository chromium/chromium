// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/filters/fe_composite.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_offset.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class FECompositeTest : public testing::Test {
 protected:
  FEComposite* CreateComposite(CompositeOperationType type,
                               float k1 = 0,
                               float k2 = 0,
                               float k3 = 0,
                               float k4 = 0) {
    // Use big filter region to avoid it from affecting FEComposite's MapRect
    // results.
    FloatRect filter_region(-10000, -10000, 20000, 20000);
    auto* filter = MakeGarbageCollected<Filter>(FloatRect(), filter_region, 1,
                                                Filter::kUserSpace);

    // Input 1 of composite has a fixed output rect.
    auto* source_graphic1 = MakeGarbageCollected<SourceGraphic>(filter);
    source_graphic1->SetClipsToBounds(false);
    source_graphic1->SetSourceRect(kInput1Rect);

    // Input 2 of composite will pass composite->MapRect()'s parameter as its
    // output.
    auto* source_graphic2 = MakeGarbageCollected<SourceGraphic>(filter);
    source_graphic2->SetClipsToBounds(false);

    // Composite input 1 and input 2.
    auto* composite =
        MakeGarbageCollected<FEComposite>(filter, type, k1, k2, k3, k4);
    composite->SetClipsToBounds(false);
    composite->InputEffects().push_back(source_graphic1);
    composite->InputEffects().push_back(source_graphic2);
    return composite;
  }

  const IntRect kInput1Rect = {50, -50, 100, 100};
};

#define EXPECT_INTERSECTION(c)                                  \
  do {                                                          \
    EXPECT_TRUE(c->MapRect(FloatRect()).IsEmpty());             \
    EXPECT_TRUE(c->MapRect(FloatRect(0, 0, 50, 50)).IsEmpty()); \
    EXPECT_EQ(FloatRect(50, 0, 100, 50),                        \
              c->MapRect(FloatRect(0, 0, 200, 200)));           \
  } while (false)

#define EXPECT_INPUT1(c)                                                      \
  do {                                                                        \
    EXPECT_EQ(FloatRect(kInput1Rect), c->MapRect(FloatRect()));               \
    EXPECT_EQ(FloatRect(kInput1Rect), c->MapRect(FloatRect(0, 0, 50, 50)));   \
    EXPECT_EQ(FloatRect(kInput1Rect), c->MapRect(FloatRect(0, 0, 200, 200))); \
  } while (false)

#define EXPECT_INPUT2(c)                                                     \
  do {                                                                       \
    EXPECT_TRUE(c->MapRect(FloatRect()).IsEmpty());                          \
    EXPECT_EQ(FloatRect(0, 0, 50, 50), c->MapRect(FloatRect(0, 0, 50, 50))); \
    EXPECT_EQ(FloatRect(0, 0, 200, 200),                                     \
              c->MapRect(FloatRect(0, 0, 200, 200)));                        \
  } while (false)

#define EXPECT_UNION(c)                                         \
  do {                                                          \
    EXPECT_EQ(FloatRect(kInput1Rect), c->MapRect(FloatRect())); \
    EXPECT_EQ(FloatRect(0, -50, 150, 100),                      \
              c->MapRect(FloatRect(0, 0, 50, 50)));             \
    EXPECT_EQ(FloatRect(0, -50, 200, 250),                      \
              c->MapRect(FloatRect(0, 0, 200, 200)));           \
  } while (false)

#define EXPECT_EMPTY(c)                                           \
  do {                                                            \
    EXPECT_TRUE(c->MapRect(FloatRect()).IsEmpty());               \
    EXPECT_TRUE(c->MapRect(FloatRect(0, 0, 50, 50)).IsEmpty());   \
    EXPECT_TRUE(c->MapRect(FloatRect(0, 0, 200, 200)).IsEmpty()); \
  } while (false)

TEST_F(FECompositeTest, MapRectIn) {
  EXPECT_INTERSECTION(CreateComposite(FECOMPOSITE_OPERATOR_IN));
}

TEST_F(FECompositeTest, MapRectATop) {
  EXPECT_INPUT2(CreateComposite(FECOMPOSITE_OPERATOR_ATOP));
}

TEST_F(FECompositeTest, MapRectOtherOperators) {
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_OVER));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_OUT));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_XOR));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_LIGHTER));
}

TEST_F(FECompositeTest, MapRectArithmetic) {
  EXPECT_EMPTY(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 0, 0, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 0, 0, 1));
  EXPECT_INPUT2(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 0, 1, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 0, 1, 1));
  EXPECT_INPUT1(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 1, 0, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 1, 0, 1));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 1, 1, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 0, 1, 1, 1));
  EXPECT_INTERSECTION(
      CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 0, 0, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 0, 0, 1));
  EXPECT_INPUT2(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 0, 1, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 0, 1, 1));
  EXPECT_INPUT1(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 1, 0, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 1, 0, 1));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 1, 1, 0));
  EXPECT_UNION(CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 1, 1, 1));
}

TEST_F(FECompositeTest, MapRectArithmeticK4Clipped) {
  // Arithmetic operator with positive K4 will always affect the whole primitive
  // subregion.
  auto* c = CreateComposite(FECOMPOSITE_OPERATOR_ARITHMETIC, 1, 1, 1, 1);
  c->SetClipsToBounds(true);
  FloatRect bounds(222, 333, 444, 555);
  c->SetFilterPrimitiveSubregion(bounds);
  EXPECT_EQ(bounds, c->MapRect(FloatRect()));
  EXPECT_EQ(bounds, c->MapRect(FloatRect(100, 200, 300, 400)));
}

}  // namespace blink
