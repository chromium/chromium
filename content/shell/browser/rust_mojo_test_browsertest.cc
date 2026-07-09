// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
    base::FilePath gen_dir;
    base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_dir);
    // Serve the gen directory so that Mojo bindings resolve correctly.
    embedded_test_server()->ServeFilesFromDirectory(gen_dir);
    embedded_test_server()->ServeFilesFromDirectory(
        GetTestFilePath(nullptr, ""));
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

class RustMojoTestBrowserTestWithFeatureEnabled
    : public RustMojoTestBrowserTest {
 public:
  RustMojoTestBrowserTestWithFeatureEnabled() {
    // Presently, Rust's `base_feature!` macro doesn't default to exporting
    // Features declared on the Rust side, and thus the C++ side doesn't have
    // a direct way to reference them. crrev/7802270 will fix this by having
    // that macro export symbols by default, but in the meantime, we use
    // InitFromCommandLine here so we can successfully test the feature.
    feature_list_.InitFromCommandLine("FeatureFlagSetViaRust", "");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class RustMojoTestBrowserTestWithCommandLineSwitch
    : public RustMojoTestBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RustMojoTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("command-line-flag-parsable-by-rust",
                                    "hello_from_command_line");
  }
};

IN_PROC_BROWSER_TEST_F(RustMojoTestBrowserTest, LoadRustMojoService) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/rust_mojo_test.html")));
  EXPECT_EQ("Rust says hi.", EvalJs(shell(), "getRustMessage(1)"));

  // Verify that the feature flag is disabled by default.
  EXPECT_EQ(false,
            EvalJs(shell(),
                   "content.rustTest.mojom.RustTestService.getRemote()."
                   "isFeatureFlagSetViaRustEnabled().then(r => r.enabled)"));

  // Verify that command line switches can be read from Rust.
  EXPECT_EQ(
      true,
      EvalJs(
          shell(),
          "content.rustTest.mojom.RustTestService.getRemote()."
          "hasCommandLineSwitch('enable-blink-features').then(r => r.exists)"));
  EXPECT_EQ("MojoJS,MojoJSTest",
            EvalJs(shell(),
                   "content.rustTest.mojom.RustTestService.getRemote()."
                   "getCommandLineSwitchValue('enable-blink-features').then(r "
                   "=> r.value)"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "content.rustTest.mojom.RustTestService.getRemote()."
                          "hasCommandLineSwitch('command-line-flag-parsable-by-"
                          "rust').then(r => r.exists)"));
}

IN_PROC_BROWSER_TEST_F(RustMojoTestBrowserTestWithFeatureEnabled,
                       LoadRustMojoServiceWithFeatureEnabled) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/rust_mojo_test.html")));

  // Verify that the feature flag is enabled.
  EXPECT_EQ(true,
            EvalJs(shell(),
                   "content.rustTest.mojom.RustTestService.getRemote()."
                   "isFeatureFlagSetViaRustEnabled().then(r => r.enabled)"));
}

IN_PROC_BROWSER_TEST_F(RustMojoTestBrowserTestWithCommandLineSwitch,
                       LoadRustMojoServiceWithCommandLineSwitch) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/rust_mojo_test.html")));

  // Verify that custom command line switch is detected via Rust.
  EXPECT_EQ(true, EvalJs(shell(),
                         "content.rustTest.mojom.RustTestService.getRemote()."
                         "hasCommandLineSwitch('command-line-flag-parsable-by-"
                         "rust').then(r => r.exists)"));
  EXPECT_EQ("hello_from_command_line",
            EvalJs(shell(),
                   "content.rustTest.mojom.RustTestService.getRemote()."
                   "getCommandLineSwitchValue('command-line-flag-parsable-by-"
                   "rust').then(r => r.value)"));
}

}  // namespace content
