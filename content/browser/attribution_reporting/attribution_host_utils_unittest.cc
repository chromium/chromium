// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host_utils.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace attribution_host_utils {

class ConversionHostUtilsTest : public ::testing::Test {};

TEST(ConversionHostUtilsTest, AppImpression_Valid) {
  absl::optional<blink::Impression> impression =
      ParseImpressionFromApp("9223372036854775807", "https://example.com",
                             "https://example2.com", 1234);
  EXPECT_EQ(9223372036854775807ull, impression->impression_data);
  EXPECT_EQ("example.com", impression->conversion_destination.host());
  EXPECT_EQ("example2.com", impression->reporting_origin->host());
  EXPECT_EQ(base::Milliseconds(1234), impression->expiry);
}

TEST(ConversionHostUtilsTest, AppImpression_Valid_NoOptionals) {
  absl::optional<blink::Impression> impression = ParseImpressionFromApp(
      "9223372036854775807", "https://example.com", "", 0);
  EXPECT_EQ(9223372036854775807ull, impression->impression_data);
  EXPECT_EQ("example.com", impression->conversion_destination.host());
  EXPECT_EQ(absl::nullopt, impression->reporting_origin);
  EXPECT_EQ(absl::nullopt, impression->expiry);
}

TEST(ConversionHostUtilsTest, AppImpression_Invalid_Destination) {
  EXPECT_EQ(absl::nullopt,
            ParseImpressionFromApp("12345", "http://bad.com", "", 0));
}

TEST(ConversionHostUtilsTest, AppImpression_Invalid_ReportTo) {
  EXPECT_EQ(absl::nullopt,
            ParseImpressionFromApp("12345", "https://example.com",
                                   "http://bad.com", 0));
}

TEST(ConversionHostUtilsTest, AppImpression_Invalid_EventId) {
  EXPECT_EQ(absl::nullopt,
            ParseImpressionFromApp("-12345", "https://example.com", "", 0));
}

}  // namespace attribution_host_utils
}  // namespace content
