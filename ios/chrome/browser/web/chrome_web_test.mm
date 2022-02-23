// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/chrome_web_test.h"

#include "base/bind.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeWebTest::~ChromeWebTest() {}

ChromeWebTest::ChromeWebTest(std::unique_ptr<web::WebClient> web_client,
                             web::WebTaskEnvironment::Options options)
    : web::WebTestWithWebState(std::move(web_client), options) {}

ChromeWebTest::ChromeWebTest(web::WebTaskEnvironment::Options options)
    : web::WebTestWithWebState(options) {}

void ChromeWebTest::TearDown() {
  WaitForBackgroundTasks();
  web::WebTestWithWebState::TearDown();
}

std::unique_ptr<web::BrowserState> ChromeWebTest::CreateBrowserState() {
  TestChromeBrowserState::Builder test_cbs_builder;
  test_cbs_builder.AddTestingFactory(
      IOSChromePasswordStoreFactory::GetInstance(),
      base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                          web::BrowserState,
                          password_manager::MockPasswordStoreInterface>));
  for (auto& pair : GetTestingFactories()) {
    test_cbs_builder.AddTestingFactory(pair.first, pair.second);
  }
  return test_cbs_builder.Build();
}

TestChromeBrowserState::TestingFactories ChromeWebTest::GetTestingFactories() {
  return {};
}

TestChromeBrowserState* ChromeWebTest::GetBrowserState() {
  return static_cast<TestChromeBrowserState*>(
      web::WebTestWithWebState::GetBrowserState());
}
