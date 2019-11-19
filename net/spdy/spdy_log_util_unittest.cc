// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_log_util.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

std::string ElideGoAwayDebugDataForNetLogAsString(
    NetLogCaptureMode capture_mode,
    base::StringPiece debug_data) {
  auto value = ElideGoAwayDebugDataForNetLog(capture_mode, debug_data);
  std::string str;
  EXPECT_TRUE(value.GetAsString(&str));
  return str;
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

TEST(SpdyLogUtilTest, ElideSpdyHeaderBlockForNetLog) {
  spdy::SpdyHeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  base::ListValue list =
      ElideSpdyHeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  ASSERT_FALSE(list.is_none());
  ASSERT_EQ(2u, list.GetList().size());

  ASSERT_TRUE(list.GetList()[0].is_string());
  EXPECT_EQ("foo: bar", list.GetList()[0].GetString());

  ASSERT_TRUE(list.GetList()[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]", list.GetList()[1].GetString());

  list = ElideSpdyHeaderBlockForNetLog(headers,
                                       NetLogCaptureMode::kIncludeSensitive);

  ASSERT_FALSE(list.is_none());
  ASSERT_EQ(2u, list.GetList().size());

  ASSERT_TRUE(list.GetList()[0].is_string());
  EXPECT_EQ("foo: bar", list.GetList()[0].GetString());

  ASSERT_TRUE(list.GetList()[1].is_string());
  EXPECT_EQ("cookie: name=value", list.GetList()[1].GetString());
}

TEST(SpdyLogUtilTest, SpdyHeaderBlockNetLogParams) {
  spdy::SpdyHeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  std::unique_ptr<base::Value> dict = base::Value::ToUniquePtrValue(
      SpdyHeaderBlockNetLogParams(&headers, NetLogCaptureMode::kDefault));

  ASSERT_TRUE(dict);
  ASSERT_TRUE(dict->is_dict());
  ASSERT_EQ(1u, dict->DictSize());

  auto* header_list = dict->FindKey("headers");
  ASSERT_TRUE(header_list);
  ASSERT_TRUE(header_list->is_list());
  ASSERT_EQ(2u, header_list->GetList().size());

  ASSERT_TRUE(header_list->GetList()[0].is_string());
  EXPECT_EQ("foo: bar", header_list->GetList()[0].GetString());

  ASSERT_TRUE(header_list->GetList()[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]",
            header_list->GetList()[1].GetString());

  dict = base::Value::ToUniquePtrValue(SpdyHeaderBlockNetLogParams(
      &headers, NetLogCaptureMode::kIncludeSensitive));

  ASSERT_TRUE(dict);
  ASSERT_TRUE(dict->is_dict());
  ASSERT_EQ(1u, dict->DictSize());

  header_list = dict->FindKey("headers");
  ASSERT_TRUE(header_list);
  ASSERT_TRUE(header_list->is_list());
  ASSERT_EQ(2u, header_list->GetList().size());

  ASSERT_TRUE(header_list->GetList()[0].is_string());
  EXPECT_EQ("foo: bar", header_list->GetList()[0].GetString());

  ASSERT_TRUE(header_list->GetList()[1].is_string());
  EXPECT_EQ("cookie: name=value", header_list->GetList()[1].GetString());
}

// Regression test for https://crbug.com/800282.
TEST(SpdyLogUtilTest, ElideSpdyHeaderBlockForNetLogWithNonUTF8Characters) {
  spdy::SpdyHeaderBlock headers;
  headers["foo"] = "bar\x81";
  headers["O\xe2"] = "bar";
  headers["\xde\xad"] = "\xbe\xef";

  base::ListValue list =
      ElideSpdyHeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  ASSERT_EQ(3u, list.GetSize());
  std::string field;
  EXPECT_TRUE(list.GetString(0, &field));
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B foo: bar%81", field);
  EXPECT_TRUE(list.GetString(1, &field));
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B O%E2: bar", field);
  EXPECT_TRUE(list.GetString(2, &field));
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %DE%AD: %BE%EF", field);
}

}  // namespace net
