// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test class for the IncognitoLockMediator.
class IncognitoLockMediatorTest : public PlatformTest {
 protected:
  IncognitoLockMediatorTest() {
    feature_list_.InitWithFeatureState(kIOSSoftLock, YES);
    consumer_ = [OCMockObject mockForProtocol:@protocol(IncognitoLockConsumer)];
  }

  void SetUp() override {
    local_state_.registry()->RegisterBooleanPref(
        prefs::kIncognitoAuthenticationSetting, false);
    local_state_.registry()->RegisterBooleanPref(
        prefs::kIncognitoSoftLockSetting, true);
    mediator_ =
        [[IncognitoLockMediator alloc] initWithLocalState:&local_state_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    // Clearing prefs after disconnecting the mediaor so the consumer doesn't
    // get notified with false information on teardown.
    local_state_.ClearPref(prefs::kIncognitoSoftLockSetting);
    local_state_.ClearPref(prefs::kIncognitoAuthenticationSetting);
  }

  web::WebTaskEnvironment task_environment_;
  IncognitoLockMediator* mediator_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;

  // Mocks
  id consumer_;
};

// Tests that the consumer is correctly notified when the mediator is initiated
// and no lock is required.
TEST_F(IncognitoLockMediatorTest, InitWithoutLock) {
  OCMExpect([consumer_ setIncognitoLockState:IncognitoLockState::kNone]);
  local_state_.SetBoolean(prefs::kIncognitoAuthenticationSetting, false);
  local_state_.SetBoolean(prefs::kIncognitoSoftLockSetting, false);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is correctly notified when the mediator is initiated
// and soft lock is required.
TEST_F(IncognitoLockMediatorTest, InitWithSoftLock) {
  OCMExpect([consumer_ setIncognitoLockState:IncognitoLockState::kSoftLock]);
  local_state_.SetBoolean(prefs::kIncognitoAuthenticationSetting, false);
  local_state_.SetBoolean(prefs::kIncognitoSoftLockSetting, true);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is correctly notified when the mediator is initiated
// and reauth is required.
TEST_F(IncognitoLockMediatorTest, InitWithReauth_OnlyReauthTrue) {
  OCMExpect([consumer_ setIncognitoLockState:IncognitoLockState::kReauth]);
  local_state_.SetBoolean(prefs::kIncognitoAuthenticationSetting, true);
  local_state_.SetBoolean(prefs::kIncognitoSoftLockSetting, false);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is correctly notified when the mediator is initiated
// and reauth is required when both prefs are set to true.
// This is an edge case when the user has already enabled biometric reauth then
// is migrated to the soft lock experiment they will get the initial value for
// the soft lock pref as true and the existing reauth pref will also be true.
TEST_F(IncognitoLockMediatorTest, InitWithReauth_BothPrefsTrue) {
  OCMExpect([consumer_ setIncognitoLockState:IncognitoLockState::kReauth]);
  local_state_.SetBoolean(prefs::kIncognitoAuthenticationSetting, true);
  local_state_.SetBoolean(prefs::kIncognitoSoftLockSetting, true);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}
