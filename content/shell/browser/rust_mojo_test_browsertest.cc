// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

class RustMojoTestBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MojoJS,MojoJSTest");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    base::FilePath out_dir;
    base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &out_dir);
    // Serve the build directory so that /gen/ paths resolve correctly.
    embedded_test_server()->ServeFilesFromDirectory(out_dir);
    embedded_test_server()->ServeFilesFromDirectory(
        GetTestFilePath(nullptr, ""));
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(RustMojoTestBrowserTest, LoadRustMojoService) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/rust_mojo_test.html")));
  EXPECT_EQ("Rust says hi.", EvalJs(shell(), "getRustMessage(1)"));
}

}  // namespace content
