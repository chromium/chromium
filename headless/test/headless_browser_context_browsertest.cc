// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "headless/test/headless_devtooled_browsertest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {
namespace {
const char kMainPageCookie[] = "mood=quizzical";
const char kIsolatedPageCookie[] = "mood=quixotic";
}  // namespace

// This test creates two tabs pointing to the same security origin in two
// different browser contexts and checks that they are isolated by creating two
// cookies with the same name in both tabs. The steps are:
//
// 1. Wait for tab #1 to become ready for DevTools.
// 2. Create tab #2 and wait for it to become ready for DevTools.
// 3. Navigate tab #1 to the test page and wait for it to finish loading.
// 4. Navigate tab #2 to the test page and wait for it to finish loading.
// 5. Set a cookie in tab #1.
// 6. Set the same cookie in tab #2 to a different value.
// 7. Read the cookie in tab #1 and check that it has the first value.
// 8. Read the cookie in tab #2 and check that it has the second value.
//
// If the tabs aren't properly isolated, step 7 will fail.
class HeadlessBrowserContextIsolationTest
    : public HeadlessDevTooledBrowserTest {
 public:
  HeadlessBrowserContextIsolationTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override {
    if (!web_contents2_) {
      browser_context_ = browser()->CreateBrowserContextBuilder().Build();
      web_contents2_ = browser_context_->CreateWebContentsBuilder().Build();
      web_contents2_->AddObserver(this);
      return;
    }

    devtools_client2_.AttachToWebContents(
        HeadlessWebContentsImpl::From(web_contents2_)->web_contents());

    HeadlessDevTooledBrowserTest::DevToolsTargetReady();
  }

  void RunDevTooledTest() override {
    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &HeadlessBrowserContextIsolationTest::OnFirstLoadEventFired,
            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnFirstLoadEventFired(const base::Value::Dict&) {
    devtools_client2_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &HeadlessBrowserContextIsolationTest::OnSecondLoadEventFired,
            base::Unretained(this)));
    SendCommandSync(devtools_client2_, "Page.enable");

    devtools_client2_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnSecondLoadEventFired(const base::Value::Dict&) {
    // Set cookies on both pages.
    EXPECT_THAT(EvaluateScript(web_contents_,
                               base::StringPrintf("document.cookie = '%s'",
                                                  kMainPageCookie)),
                DictHasValue("result.result.value", kMainPageCookie));

    EXPECT_THAT(EvaluateScript(web_contents2_,
                               base::StringPrintf("document.cookie = '%s'",
                                                  kIsolatedPageCookie)),
                DictHasValue("result.result.value", kIsolatedPageCookie));

    // Get cookies from both pages and verify.
    EXPECT_THAT(EvaluateScript(web_contents_, "document.cookie"),
                DictHasValue("result.result.value", kMainPageCookie));

    EXPECT_THAT(EvaluateScript(web_contents2_, "document.cookie"),
                DictHasValue("result.result.value", kIsolatedPageCookie));

    web_contents2_->RemoveObserver(this);
    web_contents2_->Close();
    browser_context_->Close();

    FinishAsynchronousTest();
  }

 private:
  raw_ptr<HeadlessBrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_ = nullptr;
  raw_ptr<HeadlessWebContents, AcrossTasksDanglingUntriaged> web_contents2_ =
      nullptr;
  SimpleDevToolsProtocolClient devtools_client2_;
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessBrowserContextIsolationTest);

class HeadlessBrowserUserDataDirTest : public HeadlessBrowserTest {
 protected:
  HeadlessBrowserUserDataDirTest() = default;
  ~HeadlessBrowserUserDataDirTest() override = default;
  HeadlessBrowserUserDataDirTest(const HeadlessBrowserUserDataDirTest&) =
      delete;
  HeadlessBrowserUserDataDirTest& operator=(
      const HeadlessBrowserUserDataDirTest&) = delete;

  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  // HeadlessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    // The newly created temp directory should be empty.
    EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir()));

    HeadlessBrowserTest::SetUp();
  }

 private:
  base::ScopedTempDir user_data_dir_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUserDataDirTest, Do) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context = browser()
                                                ->CreateBrowserContextBuilder()
                                                .SetUserDataDir(user_data_dir())
                                                .SetIncognitoMode(false)
                                                .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();

  EXPECT_TRUE(WaitForLoad(web_contents));

  // Something should be written to this directory.
  // If it is not the case, more complex page may be needed.
  // ServiceWorkers may be a good option.
  EXPECT_FALSE(base::IsDirectoryEmpty(user_data_dir()));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, IncognitoMode) {
  // We do not want to bother with posting tasks to create a temp dir.
  // Just allow blocking from main thread for now.
  base::ScopedAllowBlockingForTesting allow_blocking;

  EXPECT_TRUE(embedded_test_server()->Start());

  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());

  // Newly created temp directory should be empty.
  EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir.GetPath()));

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetUserDataDir(user_data_dir.GetPath())
          .SetIncognitoMode(true)
          .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();

  EXPECT_TRUE(WaitForLoad(web_contents));

  // Similar to test above, but now we are in incognito mode,
  // so nothing should be written to this directory.
  EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir.GetPath()));
}

class HeadlessBrowserTestWithUserDataDirAndMaybeIncognito
    : public HeadlessBrowserTest,
      public testing::WithParamInterface<bool> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::IsDirectoryEmpty(user_data_dir()));
  }

 protected:
  bool incognito() { return GetParam(); }

  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

 private:
  base::ScopedTempDir user_data_dir_;
};

INSTANTIATE_TEST_SUITE_P(HeadlessBrowserTestWithUserDataDirAndMaybeIncognito,
                         HeadlessBrowserTestWithUserDataDirAndMaybeIncognito,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithUserDataDirAndMaybeIncognito,
                       IncognitoSwitch) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context = browser()
                                                ->CreateBrowserContextBuilder()
                                                .SetUserDataDir(user_data_dir())
                                                .SetIncognitoMode(incognito())
                                                .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();

  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(incognito(), base::IsDirectoryEmpty(user_data_dir()));
}

}  // namespace headless
