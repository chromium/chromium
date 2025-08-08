// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_content_browser_client.h"

#include <string_view>

#include "base/test/scoped_command_line.h"
#include "fuchsia_web/webengine/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class WebEngineContentBrowserClientTest : public testing::Test {
 protected:
  void OverrideProtectedServiceWorkers(std::string_view value) {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kProtectedServiceWorkers, value);
  }

  void OverrideProtectedServiceWorkers() {
    OverrideProtectedServiceWorkers("http://[*.]example.com/");
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(WebEngineContentBrowserClientTest, ProtectedServiceWorkers_Matches) {
  OverrideProtectedServiceWorkers();
  ASSERT_FALSE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://www.example.com/scope"),
          /* browser_context = */ nullptr));
}

TEST_F(WebEngineContentBrowserClientTest,
       ProtectedServiceWorkers_Matches_Root) {
  OverrideProtectedServiceWorkers();
  ASSERT_FALSE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://www.example.com/"),
          /* browser_context = */ nullptr));
}

TEST_F(WebEngineContentBrowserClientTest,
       ProtectedServiceWorkers_Matches_SameDomain) {
  OverrideProtectedServiceWorkers();
  ASSERT_FALSE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://prerelease.example.com/scope"),
          /* browser_context = */ nullptr));
}

TEST_F(WebEngineContentBrowserClientTest, ProtectedServiceWorkers_Unmatches) {
  OverrideProtectedServiceWorkers();
  ASSERT_TRUE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://www.google.com/scope"),
          /* browser_context = */ nullptr));
}

TEST_F(WebEngineContentBrowserClientTest,
       ProtectedServiceWorkers_MatchesWithMultipleScopes) {
  OverrideProtectedServiceWorkers(
      "http://[*.]example.com,http://[*.]google.com");
  ASSERT_FALSE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://www.example.com/scope"),
          /* browser_context = */ nullptr));
  ASSERT_FALSE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://www.google.com/scope"),
          /* browser_context = */ nullptr));
}

TEST_F(WebEngineContentBrowserClientTest,
       ProtectedServiceWorkers_UnmatchesDomain) {
  OverrideProtectedServiceWorkers();
  ASSERT_TRUE(
      WebEngineContentBrowserClient().MayDeleteServiceWorkerRegistration(
          GURL("http://wwwexample.com/scope"),
          /* browser_context = */ nullptr));
}
