// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/capture_version.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CaptureVersionTest, ToString) {
  EXPECT_EQ(media::CaptureVersion().ToString(),
            "{.source = 0, .sub_capture = 0}");
  EXPECT_EQ(media::CaptureVersion(11, 22).ToString(),
            "{.source = 11, .sub_capture = 22}");
}

// (n-1, n-1) <=> (n, n)
TEST(CaptureVersionTest, SourceLesserSubCaptureLesser) {
  const media::CaptureVersion kLhs(1, 1), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs <= kRhs);
  EXPECT_TRUE(kLhs < kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs >= kRhs);
  EXPECT_FALSE(kLhs > kRhs);
}

// (n-1, n) <=> (n, n)
TEST(CaptureVersionTest, SourceLesserSubCaptureEqual) {
  const media::CaptureVersion kLhs(1, 2), kRhs(2, 2);
  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs <= kRhs);
  EXPECT_TRUE(kLhs < kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs >= kRhs);
  EXPECT_FALSE(kLhs > kRhs);
}

// (n-1, n+1) <=> (n, n)
TEST(CaptureVersionTest, SourceLesserSubCaptureGreater) {
  const media::CaptureVersion kLhs(1, 3), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs <= kRhs);
  EXPECT_TRUE(kLhs < kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs >= kRhs);
  EXPECT_FALSE(kLhs > kRhs);
}

// (n, n-1) <=> (n, n)
TEST(CaptureVersionTest, SourceEqualSubCaptureLesser) {
  const media::CaptureVersion kLhs(2, 1), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs <= kRhs);
  EXPECT_TRUE(kLhs < kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs >= kRhs);
  EXPECT_FALSE(kLhs > kRhs);
}

// (n, n) <=> (n, n)
TEST(CaptureVersionTest, SourceEqualSubCaptureEqual) {
  const media::CaptureVersion kLhs(2, 2), kRhs(2, 2);

  EXPECT_TRUE(kLhs == kRhs);
  EXPECT_TRUE(kLhs <= kRhs);
  EXPECT_TRUE(kLhs >= kRhs);

  EXPECT_FALSE(kLhs != kRhs);
  EXPECT_FALSE(kLhs < kRhs);
  EXPECT_FALSE(kLhs > kRhs);
}

// (n, n+1) <=> (n, n)
TEST(CaptureVersionTest, SourceEqualSubCaptureGreater) {
  const media::CaptureVersion kLhs(2, 3), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs >= kRhs);
  EXPECT_TRUE(kLhs > kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs <= kRhs);
  EXPECT_FALSE(kLhs < kRhs);
}

// (n+1, n-1) <=> (n, n)
TEST(CaptureVersionTest, SourceGreaterSubCaptureLesser) {
  const media::CaptureVersion kLhs(3, 1), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs >= kRhs);
  EXPECT_TRUE(kLhs > kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs <= kRhs);
  EXPECT_FALSE(kLhs < kRhs);
}

// (n+1, n) <=> (n, n)
TEST(CaptureVersionTest, SourceGreaterSubCaptureEqual) {
  const media::CaptureVersion kLhs(3, 2), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs >= kRhs);
  EXPECT_TRUE(kLhs > kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs <= kRhs);
  EXPECT_FALSE(kLhs < kRhs);
}

// (n+1, n+1) <=> (n, n)
TEST(CaptureVersionTest, SourceGreaterSubCaptureGreater) {
  const media::CaptureVersion kLhs(3, 3), kRhs(2, 2);

  EXPECT_TRUE(kLhs != kRhs);
  EXPECT_TRUE(kLhs >= kRhs);
  EXPECT_TRUE(kLhs > kRhs);

  EXPECT_FALSE(kLhs == kRhs);
  EXPECT_FALSE(kLhs <= kRhs);
  EXPECT_FALSE(kLhs < kRhs);
}

}  // namespace
