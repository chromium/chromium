// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace safe_browsing {

class FakeChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit FakeChromePasswordProtectionService(
      TestChromeBrowserState* browser_state)
      : ChromePasswordProtectionService(browser_state) {}

 protected:
  friend class ChromePasswordProtectionServiceTest;
};

class ChromePasswordProtectionServiceTest : public PlatformTest {
 public:
  ChromePasswordProtectionServiceTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {
    service_ = std::make_unique<FakeChromePasswordProtectionService>(
        browser_state_.get());
  }

  ~ChromePasswordProtectionServiceTest() override {}

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<FakeChromePasswordProtectionService> service_;
};

}  // namespace safe_browsing
