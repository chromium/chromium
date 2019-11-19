// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_log_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpLogUtilTest, ElideHeaderValueForNetLog) {
  // Only elide for appropriate log level.
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault, "Cookie",
                                      "name=value"));
  EXPECT_EQ("name=value",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kIncludeSensitive,
                                      "Cookie", "name=value"));

  // Headers are compared case insensitively.
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault, "cOoKiE",
                                      "name=value"));

  // These headers should be completely elided.
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault, "Set-Cookie",
                                      "name=value"));
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "Set-Cookie2", "name=value"));
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "Authorization", "Basic 1234"));
  EXPECT_EQ("[10 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "Proxy-Authorization", "Basic 1234"));

  // Unknown headers should pass through.
  EXPECT_EQ("value", ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                               "Boring", "value"));

  // Basic and Digest auth challenges are public.
  EXPECT_EQ("Basic realm=test",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "WWW-Authenticate", "Basic realm=test"));
  EXPECT_EQ("Digest realm=test",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "WWW-Authenticate", "Digest realm=test"));
  EXPECT_EQ("Basic realm=test", ElideHeaderValueForNetLog(
                                    NetLogCaptureMode::kDefault,
                                    "Proxy-Authenticate", "Basic realm=test"));
  EXPECT_EQ(
      "Digest realm=test",
      ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                "Proxy-Authenticate", "Digest realm=test"));

  // Multi-round mechanisms partially elided.
  EXPECT_EQ("NTLM [4 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "WWW-Authenticate", "NTLM 1234"));
  EXPECT_EQ("NTLM [4 bytes were stripped]",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "Proxy-Authenticate", "NTLM 1234"));

  // Leave whitespace intact.
  EXPECT_EQ("NTLM  [4 bytes were stripped] ",
            ElideHeaderValueForNetLog(NetLogCaptureMode::kDefault,
                                      "WWW-Authenticate", "NTLM  1234 "));
}

}  // namespace net
