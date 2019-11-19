// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHROME_WEB_TEST_H_
#define IOS_CHROME_BROWSER_WEB_CHROME_WEB_TEST_H_

#include <memory>

#import "ios/web/public/test/web_test_with_web_state.h"

class TestChromeBrowserState;

namespace web {
class WebClient;
}  // namespace web

// Test fixture that exposes a TestChromeBrowserState to allow configuring
// the BrowserState in tests.
class ChromeWebTest : public web::WebTestWithWebState {
 public:
  ~ChromeWebTest() override;

 protected:
  ChromeWebTest(web::WebTaskEnvironment::Options =
                    web::WebTaskEnvironment::Options::DEFAULT);
  explicit ChromeWebTest(std::unique_ptr<web::WebClient> web_client,
                         web::WebTaskEnvironment::Options =
                             web::WebTaskEnvironment::Options::DEFAULT);
  // WebTest implementation.
  void SetUp() override;
  void TearDown() override;
  web::BrowserState* GetBrowserState() override;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

#endif  // IOS_CHROME_BROWSER_WEB_CHROME_WEB_TEST_H_
