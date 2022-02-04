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

TEST(SpdyLogUtilTest, ElideHttp2HeaderBlockForNetLog) {
  spdy::Http2HeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  base::ListValue list =
      ElideHttp2HeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  ASSERT_FALSE(list.is_none());
  ASSERT_EQ(2u, list.GetListDeprecated().size());

  ASSERT_TRUE(list.GetListDeprecated()[0].is_string());
  EXPECT_EQ("foo: bar", list.GetListDeprecated()[0].GetString());

  ASSERT_TRUE(list.GetListDeprecated()[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]",
            list.GetListDeprecated()[1].GetString());

  list = ElideHttp2HeaderBlockForNetLog(headers,
                                        NetLogCaptureMode::kIncludeSensitive);

  ASSERT_FALSE(list.is_none());
  ASSERT_EQ(2u, list.GetListDeprecated().size());

  ASSERT_TRUE(list.GetListDeprecated()[0].is_string());
  EXPECT_EQ("foo: bar", list.GetListDeprecated()[0].GetString());

  ASSERT_TRUE(list.GetListDeprecated()[1].is_string());
  EXPECT_EQ("cookie: name=value", list.GetListDeprecated()[1].GetString());
}

TEST(SpdyLogUtilTest, Http2HeaderBlockNetLogParams) {
  spdy::Http2HeaderBlock headers;
  headers["foo"] = "bar";
  headers["cookie"] = "name=value";

  std::unique_ptr<base::Value> dict = base::Value::ToUniquePtrValue(
      Http2HeaderBlockNetLogParams(&headers, NetLogCaptureMode::kDefault));

  ASSERT_TRUE(dict);
  ASSERT_TRUE(dict->is_dict());
  ASSERT_EQ(1u, dict->DictSize());

  auto* header_list = dict->FindKey("headers");
  ASSERT_TRUE(header_list);
  ASSERT_TRUE(header_list->is_list());
  ASSERT_EQ(2u, header_list->GetListDeprecated().size());

  ASSERT_TRUE(header_list->GetListDeprecated()[0].is_string());
  EXPECT_EQ("foo: bar", header_list->GetListDeprecated()[0].GetString());

  ASSERT_TRUE(header_list->GetListDeprecated()[1].is_string());
  EXPECT_EQ("cookie: [10 bytes were stripped]",
            header_list->GetListDeprecated()[1].GetString());

  dict = base::Value::ToUniquePtrValue(Http2HeaderBlockNetLogParams(
      &headers, NetLogCaptureMode::kIncludeSensitive));

  ASSERT_TRUE(dict);
  ASSERT_TRUE(dict->is_dict());
  ASSERT_EQ(1u, dict->DictSize());

  header_list = dict->FindKey("headers");
  ASSERT_TRUE(header_list);
  ASSERT_TRUE(header_list->is_list());
  ASSERT_EQ(2u, header_list->GetListDeprecated().size());

  ASSERT_TRUE(header_list->GetListDeprecated()[0].is_string());
  EXPECT_EQ("foo: bar", header_list->GetListDeprecated()[0].GetString());

  ASSERT_TRUE(header_list->GetListDeprecated()[1].is_string());
  EXPECT_EQ("cookie: name=value",
            header_list->GetListDeprecated()[1].GetString());
}

// Regression test for https://crbug.com/800282.
TEST(SpdyLogUtilTest, ElideHttp2HeaderBlockForNetLogWithNonUTF8Characters) {
  spdy::Http2HeaderBlock headers;
  headers["foo"] = "bar\x81";
  headers["O\xe2"] = "bar";
  headers["\xde\xad"] = "\xbe\xef";

  base::ListValue list =
      ElideHttp2HeaderBlockForNetLog(headers, NetLogCaptureMode::kDefault);

  base::Value::ConstListView list_view = list.GetListDeprecated();
  ASSERT_EQ(3u, list_view.size());
  ASSERT_TRUE(list_view[0].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B foo: bar%81", list_view[0].GetString());
  ASSERT_TRUE(list_view[1].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B O%E2: bar", list_view[1].GetString());
  ASSERT_TRUE(list_view[2].is_string());
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %DE%AD: %BE%EF", list_view[2].GetString());
}

}  // namespace net
