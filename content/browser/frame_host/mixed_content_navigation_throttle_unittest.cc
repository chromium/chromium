// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/mixed_content_navigation_throttle.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Tests that MixedContentNavigationThrottle correctly detects or ignores many
// cases where there is or there is not mixed content, respectively.
// Note: Browser side version of MixedContentCheckerTest.IsMixedContent. Must be
// kept in sync manually!
TEST(MixedContentNavigationThrottleTest, IsMixedContent) {
  struct TestCase {
    const char* origin;
    const char* target;
    const bool expected_mixed_content;
  } cases[] = {
      {"http://example.com/foo", "http://example.com/foo", false},
      {"http://example.com/foo", "https://example.com/foo", false},
      {"http://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"http://example.com/foo", "about:blank", false},
      {"https://example.com/foo", "https://example.com/foo", false},
      {"https://example.com/foo", "wss://example.com/foo", false},
      {"https://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"https://example.com/foo", "http://127.0.0.1/", false},
      {"https://example.com/foo", "http://[::1]/", false},
      {"https://example.com/foo", "blob:https://example.com/foo", false},
      {"https://example.com/foo", "blob:http://example.com/foo", false},
      {"https://example.com/foo", "blob:null/foo", false},
      {"https://example.com/foo", "filesystem:https://example.com/foo", false},
      {"https://example.com/foo", "filesystem:http://example.com/foo", false},
      {"https://example.com/foo", "filesystem:null/foo", false},

      {"https://example.com/foo", "http://example.com/foo", true},
      {"https://example.com/foo", "http://google.com/foo", true},
      {"https://example.com/foo", "ws://example.com/foo", true},
      {"https://example.com/foo", "ws://google.com/foo", true},
      {"https://example.com/foo", "http://192.168.1.1/", true},
      {"https://example.com/foo", "http://localhost/", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Origin: " << test.origin << ", Target: " << test.target
                 << ", Expectation: " << test.expected_mixed_content);
    GURL origin_url(test.origin);
    GURL target_url(test.target);
    EXPECT_EQ(test.expected_mixed_content,
              MixedContentNavigationThrottle::IsMixedContentForTesting(
                  origin_url, target_url));
  }
}

}  // namespace content
