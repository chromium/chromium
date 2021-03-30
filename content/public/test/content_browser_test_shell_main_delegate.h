// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_

#include "content/shell/app/shell_main_delegate.h"

namespace content {

// Acts like normal ShellMainDelegate but inserts behaviour for browser tests.
class ContentBrowserTestShellMainDelegate : public ShellMainDelegate {
 public:
  ContentBrowserTestShellMainDelegate()
      : ShellMainDelegate(/*is_content_browsertests=*/true) {}

  // ShellMainDelegate overrides.
  content::ContentBrowserClient* CreateContentBrowserClient() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
