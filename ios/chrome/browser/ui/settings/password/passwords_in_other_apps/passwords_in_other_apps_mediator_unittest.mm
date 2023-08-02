// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_mediator.h"

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "testing/gtest/include/gtest/gtest.h"

@interface FakePasswordsInOtherAppsConsumer
    : NSObject <PasswordsInOtherAppsConsumer>

@property(nonatomic, assign) int consumerUpdatesViewControllerCount;

@end

@implementation FakePasswordsInOtherAppsConsumer

- (void)updateInstructionsWithCurrentPasswordAutoFillStatus {
  _consumerUpdatesViewControllerCount += 1;
}

@end

// Tests for Passwords in Other Apps mediator.
// Note: This test class subclasses BlockCleanupTest to deallocate the block
// passed to ASCredentialIdentityStore that updates the state of the mediator's
// passwordsAutoFillObserver.
class PasswordsInOtherAppsMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    consumer_ = [[FakePasswordsInOtherAppsConsumer alloc] init];

    mediator_ = [[PasswordsInOtherAppsMediator alloc] init];
    mediator_.consumer = consumer_;
  }

  PasswordsInOtherAppsMediator* mediator() { return mediator_; }

  FakePasswordsInOtherAppsConsumer* consumer() { return consumer_; }

 private:
  FakePasswordsInOtherAppsConsumer* consumer_;
  PasswordsInOtherAppsMediator* mediator_;
};

// Tests that the consumer is notified on password auto-fill state by the
// mediator.
TEST_F(PasswordsInOtherAppsMediatorTest,
       TestPasswordAutoFillDidChangeToStatusMethod) {
  EXPECT_EQ([consumer() consumerUpdatesViewControllerCount], 0);

  [mediator() passwordAutoFillStatusDidChange];
  ASSERT_EQ([consumer() consumerUpdatesViewControllerCount], 1);
  [mediator() passwordAutoFillStatusDidChange];
  ASSERT_EQ([consumer() consumerUpdatesViewControllerCount], 2);
}
