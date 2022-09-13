// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_EXTENSIONS_BROWSER_CLIENT_H_

#include "extensions/browser/test_extensions_browser_client.h"

namespace extensions {

// A TestExtensionsBrowserClient for AppShell tests.
class ShellTestExtensionsBrowserClient : public TestExtensionsBrowserClient {
 public:
  // If provided, |main_context| must not be an incognito context.
  explicit ShellTestExtensionsBrowserClient(
      content::BrowserContext* main_context);
  ShellTestExtensionsBrowserClient();
  ShellTestExtensionsBrowserClient(const ShellTestExtensionsBrowserClient&) =
      delete;
  ShellTestExtensionsBrowserClient& operator=(
      const ShellTestExtensionsBrowserClient&) = delete;
  ~ShellTestExtensionsBrowserClient() override;

  // ExtensionsBrowserClient overrides:
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_EXTENSIONS_BROWSER_CLIENT_H_
