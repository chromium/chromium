// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHROME_TEST_H_
#define IOS_CHROME_BROWSER_WEB_CHROME_TEST_H_

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_test.h"

namespace web {
class BrowserState;
}

// A test fixture for Chrome tests that need a minimum environment set that
// to mimics a chrome-like web embbeder. You should use this over ChromeWebTest
// if you don't need to load HTML.
class ChromeTest : public web::WebTest {
 public:
  ChromeTest();
  ~ChromeTest() override;

  // web::WebTest overrides.
  std::unique_ptr<web::BrowserState> CreateBrowserState() final;

  // This method is invoked during the creation of the BrowserState. It is
  // passed the builder that will be used for the creation. Sub-classes can
  // override the method to customize the created BrowserState.
  virtual void CustomizeBrowserStateBuilder(
      TestChromeBrowserState::Builder& builder);

  // Convenience overload for GetBrowserState() that exposes
  // TestChromeBrowserState.
  TestChromeBrowserState* GetBrowserState();
};

#endif  // IOS_CHROME_BROWSER_WEB_CHROME_TEST_H_
