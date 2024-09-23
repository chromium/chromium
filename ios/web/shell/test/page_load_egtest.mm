// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {
const char kHtmlFile[] = "/chromium_logo_page.html";
}  // namespace

// Page state test cases for the web shell.
@interface PageLoadTestCase : WebShellTestCase
@end

@implementation PageLoadTestCase

// Tests that a simple page loads successfully.
// TODO(crbug.com/354699341): Test is flaky on iPad device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testPageLoad testPageLoad
#else
#define MAYBE_testPageLoad DISABLED_testPageLoad
#endif
- (void)MAYBE_testPageLoad {
  const GURL pageURL = self.testServer->GetURL(kHtmlFile);

  [ShellEarlGrey loadURL:pageURL];
  [ShellEarlGrey waitForWebStateContainingText:
                     @"Page with some text and the chromium logo image."];
}

@end
