// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_ui_loading_util.h"

#include <optional>

#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
struct RangeTestCase {
  std::optional<std::string> range;
  std::optional<GetRequestedRangeError> error;
};
}  // namespace

class WebUILoadingUtilTest : public ::testing::TestWithParam<RangeTestCase> {};

TEST_P(WebUILoadingUtilTest, GetRequestedRange) {
  net::HttpRequestHeaders headers;
  if (GetParam().range) {
    headers.SetHeader(net::HttpRequestHeaders::kRange, *GetParam().range);
  }
  base::expected<net::HttpByteRange, GetRequestedRangeError> result =
      GetRequestedRange(headers);
  if (GetParam().error) {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), *GetParam().error);
  } else {
    EXPECT_TRUE(result.has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    WebUILoadingUtilTest,
    WebUILoadingUtilTest,
    ::testing::Values(
        RangeTestCase(std::nullopt,
                      std::make_optional(GetRequestedRangeError::kNoRanges)),
        RangeTestCase(std::make_optional("bytes=1-9"), std::nullopt),
        RangeTestCase(
            std::make_optional("bytes=1-9,11-19"),
            std::make_optional(GetRequestedRangeError::kMultipleRanges)),
        RangeTestCase(std::make_optional("bytes=3-2"),
                      std::make_optional(GetRequestedRangeError::kParseFailed)),
        RangeTestCase(
            std::make_optional("byt"),
            std::make_optional(GetRequestedRangeError::kParseFailed))));

}  // namespace content
