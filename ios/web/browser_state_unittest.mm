// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browser_state.h"

#import <WebKit/WebKit.h>

#import "base/supports_user_data.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {
class TestSupportsUserData : public base::SupportsUserData {
 public:
  TestSupportsUserData() {}
  ~TestSupportsUserData() override {}
};
}  // namespace

using BrowserStateTest = PlatformTest;

TEST_F(BrowserStateTest, FromSupportsUserData_NullPointer) {
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(nullptr));
}

TEST_F(BrowserStateTest, FromSupportsUserData_NonBrowserState) {
  TestSupportsUserData supports_user_data;
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(&supports_user_data));
}

TEST_F(BrowserStateTest, FromSupportsUserData) {
  web::FakeBrowserState browser_state;
  DCHECK_EQ(&browser_state,
            web::BrowserState::FromSupportsUserData(&browser_state));
}
