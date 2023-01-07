// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_content_browser_client.h"

#include <string>

#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that the app_shell user agent looks like a Chrome user agent.
TEST(ShellContentBrowserClientTest, UserAgentFormat) {
  ShellContentBrowserClient client(nullptr);
  std::string user_agent = client.GetUserAgent();

  // Must start with the usual Mozilla-compatibility string.
  EXPECT_TRUE(base::StartsWith(user_agent, "Mozilla/5.0",
                               base::CompareCase::INSENSITIVE_ASCII))
      << user_agent;

  // Must contain a substring like "Chrome/1.2.3.4".
  EXPECT_TRUE(base::MatchPattern(user_agent, "*Chrome/*.*.*.*")) << user_agent;
}

}  // namespace extensions
