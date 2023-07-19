// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_DEVTOOLED_BROWSERTEST_H_
#define HEADLESS_TEST_HEADLESS_DEVTOOLED_BROWSERTEST_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/test/browser_test_base.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"

namespace headless {

// Base class for tests that require access to a DevToolsClient. Subclasses
// should override the RunDevTooledTest() method, which is called asynchronously
// when the DevToolsClient is ready.
class HeadlessDevTooledBrowserTest : public HeadlessBrowserTest,
                                     public HeadlessWebContents::Observer {
 public:
  HeadlessDevTooledBrowserTest();
  ~HeadlessDevTooledBrowserTest() override;

 protected:
  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override;
  void RenderProcessExited(base::TerminationStatus status,
                           int exit_code) override;

  // Implemented by tests and used to send request(s) to DevTools. Subclasses
  // need to ensure that FinishAsynchronousTest() is called after response(s)
  // are processed (e.g. in a callback).
  virtual void RunDevTooledTest() = 0;

  // These are called just before and right after calling RunAsynchronousTest()
  virtual void PreRunAsynchronousTest() {}
  virtual void PostRunAsynchronousTest() {}

  // Whether to enable BeginFrameControl when creating |web_contents_|.
  virtual bool GetEnableBeginFrameControl();

  // Allows the HeadlessBrowserContext used in testing to be customized.
  virtual void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder);

  // Allows the HeadlessWebContents used in testing to be customized.
  virtual void CustomizeHeadlessWebContents(
      HeadlessWebContents::Builder& builder);

  void RunTest();

  raw_ptr<HeadlessBrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;
  raw_ptr<HeadlessWebContents, AcrossTasksDanglingUntriaged> web_contents_;
  simple_devtools_protocol_client::SimpleDevToolsProtocolClient
      devtools_client_;
  simple_devtools_protocol_client::SimpleDevToolsProtocolClient
      browser_devtools_client_;
};

#define HEADLESS_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)                     \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class HeadlessDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define HEADLESS_DEVTOOLED_TEST_P(TEST_FIXTURE_NAME)                     \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class HeadlessDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define DISABLED_HEADLESS_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)        \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, DISABLED_RunAsyncTest) { \
    RunTest();                                                       \
  }                                                                  \
  class HeadlessDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define DISABLED_HEADLESS_DEVTOOLED_TEST_P(TEST_FIXTURE_NAME)        \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE_NAME, DISABLED_RunAsyncTest) { \
    RunTest();                                                       \
  }                                                                  \
  class HeadlessDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_DEVTOOLED_BROWSERTEST_H_
