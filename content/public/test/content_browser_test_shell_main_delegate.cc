// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_shell_main_delegate.h"

#include "base/test/task_environment.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
absl::optional<int>
ContentBrowserTestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    // Browser tests on Lacros requires a non-null LacrosService.
    lacros_service_ = std::make_unique<chromeos::LacrosService>();
  }
  ShellMainDelegate::PostEarlyInitialization(invoked_in);
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ =
      std::make_unique<ContentBrowserTestShellContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
