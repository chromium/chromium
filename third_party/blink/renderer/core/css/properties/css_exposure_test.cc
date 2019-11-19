// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_exposure.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSExposureTest, IsUAExposed) {
  EXPECT_FALSE(IsUAExposed(CSSExposure::kNone));
  EXPECT_TRUE(IsUAExposed(CSSExposure::kUA));
  EXPECT_TRUE(IsUAExposed(CSSExposure::kWeb));
}

TEST(CSSExposureTest, IsWebExposed) {
  EXPECT_FALSE(IsWebExposed(CSSExposure::kNone));
  EXPECT_FALSE(IsWebExposed(CSSExposure::kUA));
  EXPECT_TRUE(IsWebExposed(CSSExposure::kWeb));
}

}  // namespace blink
