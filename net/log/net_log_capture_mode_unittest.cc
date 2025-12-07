// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

#include <string>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

TEST(NetLogCaptureMode, Default) {
  NetLogCaptureMode mode = NetLogCaptureMode::kDefault;

  EXPECT_FALSE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_FALSE(NetLogCaptureIncludesSocketBytes(mode));
}

TEST(NetLogCaptureMode, IncludeSensitive) {
  NetLogCaptureMode mode = NetLogCaptureMode::kIncludeSensitive;

  EXPECT_TRUE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_FALSE(NetLogCaptureIncludesSocketBytes(mode));
}

TEST(NetLogCaptureMode, Everything) {
  NetLogCaptureMode mode = NetLogCaptureMode::kEverything;

  EXPECT_TRUE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_TRUE(NetLogCaptureIncludesSocketBytes(mode));
}

TEST(NetLogCaptureMode, SanitizeUrlForNetLog) {
  const struct {
    GURL input;
    std::string_view expected_output;
  } kTestCases[] = {
      {
          // Everything other than the username/password should be left alone.
          GURL("http://a.test:78/foobar?query=1#hash"),
          "http://a.test:78/foobar?query=1#hash",
      },
      {
          // Strip username/password.
          GURL("http://user:pass@a.test"),
          "http://a.test/ (credentials redacted)",
      },
      {
          // Try an HTTPS URL.
          GURL("https://user:pass@a.test:80/sup?yo#hash"),
          "https://a.test:80/sup?yo#hash (credentials redacted)",
      },
      {
          // Try an FTP URL. GURL removes references from these, so don't
          // include one.
          GURL("ftp://user:pass@a.test:80/sup?yo"),
          "ftp://a.test:80/sup?yo (credentials redacted)",
      },
      {
          // Try a non-special URL. GURL removes references from these, so don't
          // include one.
          GURL("foobar://user:pass@a.test:80/sup?yo"),
          "foobar://a.test:80/sup?yo (credentials redacted)",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input.spec());
    // Default and heavily redacted modes should hide credentials.
    EXPECT_EQ(SanitizeUrlForNetLog(test_case.input,
                                   NetLogCaptureMode::kHeavilyRedacted),
              test_case.expected_output);
    EXPECT_EQ(
        SanitizeUrlForNetLog(test_case.input, NetLogCaptureMode::kDefault),
        test_case.expected_output);

    // More permissive modes should not.
    EXPECT_EQ(SanitizeUrlForNetLog(test_case.input,
                                   NetLogCaptureMode::kIncludeSensitive),
              test_case.input.spec());
    EXPECT_EQ(
        SanitizeUrlForNetLog(test_case.input, NetLogCaptureMode::kEverything),
        test_case.input.spec());
  }
}

}  // namespace

}  // namespace net
