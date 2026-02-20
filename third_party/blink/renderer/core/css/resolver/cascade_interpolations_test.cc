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

}  // namespace blink
