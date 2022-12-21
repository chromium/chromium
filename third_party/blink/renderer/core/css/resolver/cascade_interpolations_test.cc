// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"

#include <gtest/gtest.h>

namespace blink {

TEST(CascadeInterpolationsTest, Limit) {
  constexpr size_t max = std::numeric_limits<uint8_t>::max();

  static_assert(CascadeInterpolations::kMaxEntryIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  ActiveInterpolationsMap map;

  CascadeInterpolations interpolations;
  for (size_t i = 0; i <= max; ++i) {
    interpolations.Add(&map, CascadeOrigin::kAuthor);
  }

  // At maximum
  EXPECT_FALSE(interpolations.IsEmpty());

  interpolations.Add(&map, CascadeOrigin::kAuthor);

  // Maximum + 1
  EXPECT_TRUE(interpolations.IsEmpty());
}

TEST(CascadeInterpolationsTest, Reset) {
  ActiveInterpolationsMap map;

  CascadeInterpolations interpolations;
  EXPECT_TRUE(interpolations.IsEmpty());

  interpolations.Add(&map, CascadeOrigin::kAuthor);
  EXPECT_FALSE(interpolations.IsEmpty());

  interpolations.Reset();
  EXPECT_TRUE(interpolations.IsEmpty());
}

TEST(CascadeInterpolationsTest, EncodeDecodeInterpolationPropertyID) {
  for (CSSPropertyID id : CSSPropertyIDList()) {
    EXPECT_EQ(id, DecodeInterpolationPropertyID(
                      EncodeInterpolationPosition(id, 0u, false)));
    EXPECT_EQ(id, DecodeInterpolationPropertyID(
                      EncodeInterpolationPosition(id, 255u, false)));
    EXPECT_EQ(id, DecodeInterpolationPropertyID(
                      EncodeInterpolationPosition(id, 255u, true)));
  }
}

TEST(CascadeInterpolationsTest, EncodeDecodeInterpolationIndex) {
  CSSPropertyID id = kLastCSSProperty;
  for (uint8_t index : Vector<uint8_t>({0u, 1u, 15u, 51u, 254u, 255u})) {
    EXPECT_EQ(index, DecodeInterpolationIndex(
                         EncodeInterpolationPosition(id, index, false)));
  }
}

TEST(CascadeInterpolationsTest, EncodeDecodeIsPresentationAttribute) {
  CSSPropertyID id = kLastCSSProperty;
  EXPECT_FALSE(DecodeIsPresentationAttribute(
      EncodeInterpolationPosition(id, 0u, false)));
  EXPECT_FALSE(DecodeIsPresentationAttribute(
      EncodeInterpolationPosition(id, 13u, false)));
  EXPECT_FALSE(DecodeIsPresentationAttribute(
      EncodeInterpolationPosition(id, 255u, false)));
  EXPECT_TRUE(
      DecodeIsPresentationAttribute(EncodeInterpolationPosition(id, 0u, true)));
  EXPECT_TRUE(DecodeIsPresentationAttribute(
      EncodeInterpolationPosition(id, 13u, true)));
  EXPECT_TRUE(DecodeIsPresentationAttribute(
      EncodeInterpolationPosition(id, 255u, true)));
}

}  // namespace blink
