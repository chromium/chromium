// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/test/browser_test_base.h"
#include "headless/public/headless_browser.h"

namespace base {
class RunLoop;
}

namespace headless {

// Base class for tests which require a full instance of the headless browser.
class HeadlessBrowserTest : public content::BrowserTestBase {
 public:
  HeadlessBrowserTest(const HeadlessBrowserTest&) = delete;
  HeadlessBrowserTest& operator=(const HeadlessBrowserTest&) = delete;

  // Notify that an asynchronous test is now complete and the test runner should
  // exit.
  void FinishAsynchronousTest();

 protected:
  HeadlessBrowserTest();
  ~HeadlessBrowserTest() override;

  // BrowserTestBase:
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;
#if BUILDFLAG(IS_MAC)
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override;
#endif

  // Run an asynchronous test in a nested run loop. The caller should call
  // FinishAsynchronousTest() to notify that the test should finish.
  void RunAsynchronousTest();

 protected:
  // Call this instead of SetUp() to run tests without GPU rendering (i.e.,
  // without using SwiftShader or a hardware GPU).
  void SetUpWithoutGPU();

  // Returns the browser for the test.
  HeadlessBrowser* browser() const;

  // Returns the options used by the browser. Modify with caution, since some
  // options only take effect if they were set before browser creation.
  HeadlessBrowser::Options* options() const;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
