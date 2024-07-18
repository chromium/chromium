// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_log_util.h"

#include <string_view>

#include "base/values.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

std::string ElideGoAwayDebugDataForNetLogAsString(
    NetLogCaptureMode capture_mode,
    std::string_view debug_data) {
  auto value = ElideGoAwayDebugDataForNetLog(capture_mode, debug_data);
  if (!value.is_string()) {
    ADD_FAILURE() << "'value' should be string.";
    return std::string();
  }
  return value.GetString();
}

TEST(SpdyLogUtilTest, ElideGoAwayDebugDataForNetLog) {
  // Only elide for appropriate log level.
  EXPECT_EQ("[6 bytes were stripped]",
            ElideGoAwayDebugDataForNetLogAsString(NetLogCaptureMode::kDefault,
                                                  "foobar"));
  EXPECT_EQ("foobar", ElideGoAwayDebugDataForNetLogAsString(
                          NetLogCaptureMode::kIncludeSensitive, "foobar"));
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %FE%FF",
            ElideGoAwayDebugDataForNetLogAsString(
                NetLogCaptureMode::kIncludeSensitive, "\xfe\xff\x00"));
}

TEST(SpdyLogUtilTest, ElideHttpHeaderBlockForNetLog) {
  quiche::HttpHeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  base::Value::List list =
      ElideHttpHeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  ASSERT_EQ(2u, list.size());

  ASSERT_TRUE(list[0].is_string());
  EXPECT_EQ("foo: bar", list[0].GetString());

  ASSERT_TRUE(list[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]", list[1].GetString());

  list = ElideHttpHeaderBlockForNetLog(headers,
                                       NetLogCaptureMode::kIncludeSensitive);

  ASSERT_EQ(2u, list.size());

  ASSERT_TRUE(list[0].is_string());
  EXPECT_EQ("foo: bar", list[0].GetString());

  ASSERT_TRUE(list[1].is_string());
  EXPECT_EQ("cookie: name=value", list[1].GetString());
}

TEST(SpdyLogUtilTest, HttpHeaderBlockNetLogParams) {
  quiche::HttpHeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  base::Value::Dict dict =
      HttpHeaderBlockNetLogParams(&headers, NetLogCaptureMode::kDefault);

  ASSERT_EQ(1u, dict.size());

  auto* header_list = dict.FindList("headers");
  ASSERT_TRUE(header_list);
  ASSERT_EQ(2u, header_list->size());

  ASSERT_TRUE((*header_list)[0].is_string());
  EXPECT_EQ("foo: bar", (*header_list)[0].GetString());

  ASSERT_TRUE((*header_list)[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]", (*header_list)[1].GetString());

  dict = HttpHeaderBlockNetLogParams(&headers,
                                     NetLogCaptureMode::kIncludeSensitive);

  ASSERT_EQ(1u, dict.size());

  header_list = dict.FindList("headers");
  ASSERT_TRUE(header_list);
  ASSERT_EQ(2u, header_list->size());

  ASSERT_TRUE((*header_list)[0].is_string());
  EXPECT_EQ("foo: bar", (*header_list)[0].GetString());

  ASSERT_TRUE((*header_list)[1].is_string());
  EXPECT_EQ("cookie: name=value", (*header_list)[1].GetString());
}

// Regression test for https://crbug.com/800282.
TEST(SpdyLogUtilTest, ElideHttpHeaderBlockForNetLogWithNonUTF8Characters) {
  quiche::HttpHeaderBlock headers;
  headers["foo"] = "bar\x81";
  headers["O\xe2"] = "bar";
  headers["\xde\xad"] = "\xbe\xef";

  base::Value::List list =
      ElideHttpHeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  ASSERT_EQ(3u, list.size());
  ASSERT_TRUE(list[0].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B foo: bar%81", list[0].GetString());
  ASSERT_TRUE(list[1].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B O%E2: bar", list[1].GetString());
  ASSERT_TRUE(list[2].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %DE%AD: %BE%EF", list[2].GetString());
}

}  // namespace net
