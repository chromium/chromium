// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/chrome_test.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeTest::ChromeTest() = default;

ChromeTest::~ChromeTest() = default;

std::unique_ptr<web::BrowserState> ChromeTest::CreateBrowserState() {
  TestChromeBrowserState::Builder builder;
  CustomizeBrowserStateBuilder(builder);
  return builder.Build();
}

void ChromeTest::CustomizeBrowserStateBuilder(
    TestChromeBrowserState::Builder& builder) {
  // Nothing to do by default. Sub-classes may customize this method.
}

TestChromeBrowserState* ChromeTest::GetBrowserState() {
  return static_cast<TestChromeBrowserState*>(web::WebTest::GetBrowserState());
}
