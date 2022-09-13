// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace browsertest_util {

namespace {

class ExtensionBrowsertestUtilTest : public ShellApiTest {
 public:
  ExtensionBrowsertestUtilTest() = default;

  ExtensionBrowsertestUtilTest(const ExtensionBrowsertestUtilTest&) = delete;
  ExtensionBrowsertestUtilTest& operator=(const ExtensionBrowsertestUtilTest&) =
      delete;

  ~ExtensionBrowsertestUtilTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    extension_ = LoadExtension("extension");
    ASSERT_TRUE(extension_.get());

    // Wait for the test result to ensure the extension has loaded.
    // TODO(michaelpg): Implement a real extension readiness observer.
    ResultCatcher catcher;
    ASSERT_TRUE(catcher.GetNextResult());
  }

 protected:
  const Extension* extension() const { return extension_.get(); }

 private:
  scoped_refptr<const Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackgroundPage) {
  EXPECT_EQ(extension()->id(),
            ExecuteScriptInBackgroundPage(
                browser_context(), extension()->id(),
                "window.domAutomationController.send(chrome.runtime.id);"));

  // Check that executing a script doesn't block the browser process.
  EXPECT_EQ(std::string("/") + extensions::kGeneratedBackgroundPageFilename,
            ExecuteScriptInBackgroundPage(
                browser_context(), extension()->id(),
                "chrome.runtime.getBackgroundPage(function(result) {\n"
                "  let url = new URL(result.location.href);\n"
                "  window.domAutomationController.send(url.pathname);\n"
                "});"));

  // An argument that isn't a string should cause a failure, not a hang.
  EXPECT_NONFATAL_FAILURE(
      ExecuteScriptInBackgroundPage(browser_context(), extension()->id(),
                                    "window.domAutomationController.send(3);"),
      "send(3)");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackgroundPageNoWait) {
  // Run an arbitrary script to check that we don't wait for a response.
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
      browser_context(), extension()->id(), "let foo = 0;"));

  // Run a script asynchronously that passes the test.
  ResultCatcher catcher;
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
      browser_context(), extension()->id(), "chrome.test.notifyPass();"));
  ASSERT_TRUE(catcher.GetNextResult());

  // Specifying a non-existent extension should add a non-fatal failure.
  EXPECT_NONFATAL_FAILURE(
      EXPECT_FALSE(ExecuteScriptInBackgroundPageNoWait(
          browser_context(), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "")),
      "No enabled extension with id: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

}  // namespace

}  // namespace browsertest_util
}  // namespace extensions
