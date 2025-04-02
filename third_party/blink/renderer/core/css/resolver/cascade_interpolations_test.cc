// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"

#include <gtest/gtest.h>

namespace blink {

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
    EXPECT_EQ(
        id, DecodeInterpolationPropertyID(EncodeInterpolationPosition(id, 0u)));
    EXPECT_EQ(id, DecodeInterpolationPropertyID(
                      EncodeInterpolationPosition(id, 255u)));
  }
}

TEST(CascadeInterpolationsTest, EncodeDecodeInterpolationIndex) {
  CSSPropertyID id = kLastCSSProperty;
  for (uint8_t index : Vector<uint8_t>({0u, 1u, 15u, 51u, 254u, 255u})) {
    EXPECT_EQ(index,
              DecodeInterpolationIndex(EncodeInterpolationPosition(id, index)));
  }
}

}  // namespace blink
