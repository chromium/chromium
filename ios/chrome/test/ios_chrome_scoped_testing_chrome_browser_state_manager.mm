// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"

#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/test/testing_application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeScopedTestingChromeBrowserStateManager::
    IOSChromeScopedTestingChromeBrowserStateManager(
        std::unique_ptr<ios::ChromeBrowserStateManager> browser_state_manager)
    : browser_state_manager_(std::move(browser_state_manager)),
      saved_browser_state_manager_(
          GetApplicationContext()->GetChromeBrowserStateManager()) {
  TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
      browser_state_manager_.get());
}

IOSChromeScopedTestingChromeBrowserStateManager::
    ~IOSChromeScopedTestingChromeBrowserStateManager() {
  TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
      saved_browser_state_manager_);
}
