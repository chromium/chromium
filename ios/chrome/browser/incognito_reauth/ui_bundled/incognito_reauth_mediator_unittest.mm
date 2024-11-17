// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util.h"

// Test class for the IncognitoReauthMediator.
class IncognitoReauthMediatorTest : public PlatformTest {
 protected:
  IncognitoReauthMediatorTest() {
    feature_list_.InitWithFeatureState(kIOSSoftLock, YES);
    agent_ = [OCMockObject niceMockForClass:[IncognitoReauthSceneAgent class]];
    consumer_ =
        [OCMockObject mockForProtocol:@protocol(IncognitoReauthConsumer)];
  }

  void SetUp() override {
    mediator_ = [[IncognitoReauthMediator alloc] initWithReauthAgent:agent_];
  }

  IncognitoReauthMediator* mediator_;
  base::test::ScopedFeatureList feature_list_;

  // Mocks
  id agent_;
  id consumer_;
};

// Test that the consumer is correctly notified when the mediator is initiated
// and no lock is required.
TEST_F(IncognitoReauthMediatorTest, InitWithoutLock) {
  OCMStub([agent_ incognitoLockState]).andReturn(IncognitoLockState::kNone);
  OCMExpect([consumer_ setItemsRequireAuthentication:NO
                               withPrimaryButtonText:nil
                                  accessibilityLabel:nil]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that the consumer is correctly notified when the mediator is initiated
// and soft lock is required.
TEST_F(IncognitoReauthMediatorTest, InitWithSoftLock) {
  OCMStub([agent_ incognitoLockState]).andReturn(IncognitoLockState::kSoftLock);
  OCMExpect([consumer_
      setItemsRequireAuthentication:YES
              withPrimaryButtonText:
                  l10n_util::GetNSString(
                      IDS_IOS_INCOGNITO_REAUTH_CONTINUE_IN_INCOGNITO)
                 accessibilityLabel:
                     l10n_util::GetNSString(
                         IDS_IOS_INCOGNITO_REAUTH_CONTINUE_IN_INCOGNITO)]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that the consumer is correctly notified when the mediator is initiated
// and reauth is required.
TEST_F(IncognitoReauthMediatorTest, InitWithReauth) {
  OCMStub([agent_ incognitoLockState]).andReturn(IncognitoLockState::kReauth);
  OCMExpect([consumer_
      setItemsRequireAuthentication:YES
              withPrimaryButtonText:
                  l10n_util::GetNSStringF(
                      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
                      base::SysNSStringToUTF16(
                          BiometricAuthenticationTypeString()))
                 accessibilityLabel:
                     l10n_util::GetNSStringF(
                         IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
                         base::SysNSStringToUTF16(
                             BiometricAuthenticationTypeString()))]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}
