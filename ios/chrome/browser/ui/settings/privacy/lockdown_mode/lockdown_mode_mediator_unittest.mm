// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_mediator.h"

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_consumer.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class LockdownModeMediatorTest : public PlatformTest {
 protected:
  LockdownModeMediatorTest() = default;

  web::WebTaskEnvironment task_env_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Tests that the pref and the mediator are playing nicely together.
TEST_F(LockdownModeMediatorTest, TestPref) {
  LockdownModeMediator* mediator = [[LockdownModeMediator alloc] init];

  // Check that the consumer is correctly updated when set.
  id consumer = OCMProtocolMock(@protocol(LockdownModeConsumer));
  OCMExpect([consumer setBrowserLockdownModeEnabled:NO]);
  OCMExpect([consumer setOSLockdownModeEnabled:NO]);

  mediator.consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the consumer is correctly updated when the pref is changed.
  OCMExpect([consumer setBrowserLockdownModeEnabled:YES]);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetBoolean(prefs::kBrowserLockdownModeEnabled, true);
  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the pref is correctly updated when the mediator is asked.
  [mediator didEnableBrowserLockdownMode:NO];
  EXPECT_FALSE(local_state->GetBoolean(prefs::kBrowserLockdownModeEnabled));

  [mediator disconnect];
}
