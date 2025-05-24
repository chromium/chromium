// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_shell_main_delegate.h"

#include <optional>

#include "base/test/task_environment.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/shell/browser/shell_content_browser_client.h"

namespace content {

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainDelegate(/*is_content_browsertests=*/true) {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

void ContentBrowserTestShellMainDelegate::CreateThreadPool(
    std::string_view name) {
  // Injects a test TaskTracker to watch for long-running tasks and produce a
  // useful timeout message in order to find the cause of flaky timeout tests.
  base::test::TaskEnvironment::CreateThreadPool();
}

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<ContentBrowserTestContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
