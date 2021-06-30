// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_shell_main_delegate.h"

#include "base/test/task_environment.h"
#include "content/shell/browser/shell_content_browser_client.h"

namespace content {

// Acts like normal ShellContentBrowserClient but injects a test TaskTracker to
// watch for long-running tasks and produce a useful timeout message in order to
// find the cause of flaky timeout tests.
class ContentBrowserTestShellContentBrowserClient
    : public ShellContentBrowserClient {
 public:
  bool CreateThreadPool(base::StringPiece name) override {
    base::test::TaskEnvironment::CreateThreadPool();
    return true;
  }
};

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainDelegate(/*is_content_browsertests=*/true) {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ContentBrowserTestShellMainDelegate::PostEarlyInitialization(
    bool is_running_tests) {
  // Browser tests on Lacros requires a non-null LacrosService.
  lacros_service_ = std::make_unique<chromeos::LacrosService>();
  ShellMainDelegate::PostEarlyInitialization(is_running_tests);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ =
      std::make_unique<ContentBrowserTestShellContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
