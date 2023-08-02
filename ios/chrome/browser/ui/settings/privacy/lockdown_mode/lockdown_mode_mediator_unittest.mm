// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_mediator.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class LockdownModeMediatorTest : public PlatformTest {
 protected:
  LockdownModeMediatorTest() = default;

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(prefs::kBrowserLockdownModeEnabled,
                                            /*default_value=*/false);
    prefs_->registry()->RegisterBooleanPref(prefs::kOSLockdownModeEnabled,
                                            /*default_value=*/false);
  }

  web::WebTaskEnvironment task_env_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

// Tests that the pref and the mediator are playing nicely together.
TEST_F(LockdownModeMediatorTest, TestPref) {
  CreatePrefs();
  LockdownModeMediator* mediator =
      [[LockdownModeMediator alloc] initWithUserPrefService:prefs_.get()];

  // Check that the consumer is correctly updated when set.
  id consumer = OCMProtocolMock(@protocol(LockdownModeConsumer));
  OCMExpect([consumer setBrowserLockdownModeEnabled:NO]);
  OCMExpect([consumer setOSLockdownModeEnabled:NO]);

  mediator.consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the consumer is correctly updated when the pref is changed.
  OCMExpect([consumer setBrowserLockdownModeEnabled:YES]);
  prefs_->SetBoolean(prefs::kBrowserLockdownModeEnabled, true);
  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the pref is correctly updated when the mediator is asked.
  [mediator didEnableBrowserLockdownMode:NO];
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kBrowserLockdownModeEnabled));

  [mediator disconnect];
}
