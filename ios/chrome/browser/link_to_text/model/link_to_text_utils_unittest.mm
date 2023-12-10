// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_utils.h"

#import "ios/chrome/browser/link_to_text/model/link_generation_outcome.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

typedef PlatformTest LinkToTextUtilsTest;

namespace link_to_text {

// Tests the ParseStatus utility function.
TEST_F(LinkToTextUtilsTest, ParseStatus) {
  // Valid values.
  for (int i = 0; i <= static_cast<int>(LinkGenerationOutcome::kMaxValue);
       ++i) {
    LinkGenerationOutcome expected_outcome =
        static_cast<LinkGenerationOutcome>(i);
    std::optional<double> status = static_cast<double>(i);
    EXPECT_EQ(expected_outcome, ParseStatus(status).value());
  }

  // Invalid values.
  EXPECT_FALSE(ParseStatus(std::nullopt).has_value());
  EXPECT_FALSE(ParseStatus(-1).has_value());
  EXPECT_FALSE(
      ParseStatus(static_cast<int>(LinkGenerationOutcome::kMaxValue) + 1)
          .has_value());
}

// Tests that IsLinkGenerationTimeout returns the right values based on
// different input values.
TEST_F(LinkToTextUtilsTest, IsLinkGenerationTimeout) {
  EXPECT_TRUE(IsLinkGenerationTimeout(kLinkGenerationTimeout));
  EXPECT_TRUE(
      IsLinkGenerationTimeout(kLinkGenerationTimeout + base::Milliseconds(1)));
  EXPECT_FALSE(
      IsLinkGenerationTimeout(kLinkGenerationTimeout - base::Milliseconds(1)));
}

}  // namespace link_to_text
