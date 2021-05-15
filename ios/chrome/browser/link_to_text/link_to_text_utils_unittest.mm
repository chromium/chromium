// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_utils.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/values.h"
#import "ios/chrome/browser/link_to_text/link_generation_outcome.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest LinkToTextUtilsTest;

namespace link_to_text {

// Tests the ParseStatus utility function.
TEST_F(LinkToTextUtilsTest, ParseStatus) {
  // Valid values.
  for (int i = 0; i <= static_cast<int>(LinkGenerationOutcome::kMaxValue);
       ++i) {
    LinkGenerationOutcome expected_outcome =
        static_cast<LinkGenerationOutcome>(i);
    absl::optional<double> status = static_cast<double>(i);
    EXPECT_EQ(expected_outcome, ParseStatus(status).value());
  }

  // Invalid values.
  EXPECT_FALSE(ParseStatus(absl::nullopt).has_value());
  EXPECT_FALSE(ParseStatus(-1).has_value());
  EXPECT_FALSE(
      ParseStatus(static_cast<int>(LinkGenerationOutcome::kMaxValue) + 1)
          .has_value());
}

// Tests the ParseRect utility function.
TEST_F(LinkToTextUtilsTest, ParseRect) {
  CGRect expected_rect = CGRectMake(1, 2, 3, 4);
  base::Value rect_value = base::Value(base::Value::Type::DICTIONARY);
  rect_value.SetDoubleKey("x", expected_rect.origin.x);
  rect_value.SetDoubleKey("y", expected_rect.origin.y);
  rect_value.SetDoubleKey("width", expected_rect.size.width);
  rect_value.SetDoubleKey("height", expected_rect.size.height);

  absl::optional<CGRect> opt_rect = ParseRect(&rect_value);
  ASSERT_TRUE(opt_rect.has_value());
  EXPECT_TRUE(CGRectEqualToRect(expected_rect, opt_rect.value()));

  // Invalid values.
  EXPECT_FALSE(ParseRect(nil).has_value());
  base::Value string_value = base::Value(base::Value::Type::STRING);
  EXPECT_FALSE(ParseRect(&string_value).has_value());
  base::Value empty_dict_value = base::Value(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(ParseRect(&empty_dict_value).has_value());

  base::Value copied_value = rect_value.Clone();
  copied_value.RemoveKey("x");
  EXPECT_FALSE(ParseRect(&copied_value).has_value());

  copied_value = rect_value.Clone();
  copied_value.RemoveKey("y");
  EXPECT_FALSE(ParseRect(&copied_value).has_value());

  copied_value = rect_value.Clone();
  copied_value.RemoveKey("width");
  EXPECT_FALSE(ParseRect(&copied_value).has_value());

  copied_value = rect_value.Clone();
  copied_value.RemoveKey("height");
  EXPECT_FALSE(ParseRect(&copied_value).has_value());
}

// Tests for the ParseURL utility function.
TEST_F(LinkToTextUtilsTest, ParseURL) {
  EXPECT_FALSE(ParseURL(nil).has_value());

  std::string empty_str = "";
  EXPECT_FALSE(ParseURL(&empty_str).has_value());

  std::string invalid_url_str = "abcd";
  EXPECT_FALSE(ParseURL(&invalid_url_str).has_value());

  std::string valid_url_str = "https://www.example.com/";
  absl::optional<GURL> valid_url = ParseURL(&valid_url_str);
  EXPECT_TRUE(valid_url.has_value());
  EXPECT_EQ(GURL(valid_url_str).spec(), valid_url.value().spec());
}

// Tests that IsLinkGenerationTimeout returns the right values based on
// different input values.
TEST_F(LinkToTextUtilsTest, IsLinkGenerationTimeout) {
  EXPECT_TRUE(IsLinkGenerationTimeout(
      base::TimeDelta::FromMilliseconds(kLinkGenerationTimeoutInMs)));
  EXPECT_TRUE(IsLinkGenerationTimeout(
      base::TimeDelta::FromMilliseconds(kLinkGenerationTimeoutInMs + 1)));
  EXPECT_FALSE(IsLinkGenerationTimeout(
      base::TimeDelta::FromMilliseconds(kLinkGenerationTimeoutInMs - 1)));
}

}  // namespace link_to_text
