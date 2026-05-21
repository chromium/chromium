// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.runtime extension API.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <string>

#include "extensions/browser/api_unittest.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

class ExtensionRuntimeTest : public ApiUnitTest {
 protected:
  using ApiUnitTest::RunFunction;

  // Call runtime.setUninstallURL() and verify that the call succeeds and
  // the pref store is updated.
  void SetUninstallURL(const std::string& url) {
    RunFunction(base::MakeRefCounted<RuntimeSetUninstallURLFunction>(),
                "[\"" + url + "\"]");

    // Verify that the URL was properly written to the pref store.
    EXPECT_EQ(url, GetUninstallURL());
  }

  // Call runtime.setUninstallURL() and verify that the call throws an error and
  // the pref store is not affected.
  void SetUninstallURLError(const std::string& url) {
    std::string original_url = GetUninstallURL();
    EXPECT_EQ("Invalid URL: \"" + url + "\".",
              RunFunctionAndReturnError(
                  base::MakeRefCounted<RuntimeSetUninstallURLFunction>(),
                  "[\"" + url + "\"]"));

    // Verify that the pref store was not affected.
    EXPECT_EQ(original_url, GetUninstallURL());
  }

  std::string GetUninstallURL() {
    std::string url;
    ExtensionPrefs::Get(browser_context())
        ->ReadPrefAsString(extension()->id(), "uninstall_url", &url);
    return url;
  }
};

TEST_F(ExtensionRuntimeTest, SetUninstallURL) {
  // By default extensions should have no uninstall URLs.
  EXPECT_EQ("", GetUninstallURL());

  SetUninstallURL("https://example.com");

  // Empty URL string is accepted (to remove uninstall URL).
  SetUninstallURL("");

  // URL parameters are accepted.
  SetUninstallURL("https://example.com/abcd?param=efg");

  // Trailing spaces are accepted.
  SetUninstallURL("https://other.example.com/page   ");

  // Leading spaces are accepted.
  SetUninstallURL("   https://other.example.com/some_page");

  // HTTP URLs are accepted.
  SetUninstallURL("http://insecure.example/path");

  // Ensure that only HTTP and HTTPS resources are accepted.
  SetUninstallURLError("ws://impossible");
  SetUninstallURLError("wss://impossible");
  SetUninstallURLError("about:blank");
  SetUninstallURLError("chrome://settings");
  SetUninstallURLError("://example.com");
}

}  // namespace extensions
