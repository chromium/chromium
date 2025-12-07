// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_cofirmation_presenter.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class IdleTimeoutConfirmationMediatorTest : public PlatformTest {
 protected:
  void SetUp() override { InitIdleTimeoutConfirmationMediator(); }

  void InitIdleTimeoutConfirmationMediator() {
    idle_timeout_confirmation_consumer_ =
        OCMProtocolMock(@protocol(IdleTimeoutConfirmationConsumer));
    idle_timeout_confirmation_presenter_ =
        OCMProtocolMock(@protocol(IdleTimeoutConfirmationPresenter));
    idle_timeout_confirmation_mediator_ =
        [[IdleTimeoutConfirmationMediator alloc]
            initWithPresenter:idle_timeout_confirmation_presenter_
               dialogDuration:base::Seconds(30)];
    idle_timeout_confirmation_mediator_.consumer =
        idle_timeout_confirmation_consumer_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IdleTimeoutConfirmationMediator* idle_timeout_confirmation_mediator_;
  id idle_timeout_confirmation_consumer_;
  id idle_timeout_confirmation_presenter_;
};

// Test that the countdown is set every second.
TEST_F(IdleTimeoutConfirmationMediatorTest, TestConsumerCountdownIsSet) {
  OCMExpect(
      [idle_timeout_confirmation_consumer_ setCountdown:base::Seconds(29)]);
  OCMExpect(
      [idle_timeout_confirmation_consumer_ setCountdown:base::Seconds(28)]);
  OCMExpect(
      [idle_timeout_confirmation_consumer_ setCountdown:base::Seconds(27)]);
  OCMExpect(
      [idle_timeout_confirmation_consumer_ setCountdown:base::Seconds(26)]);
  OCMExpect(
      [idle_timeout_confirmation_consumer_ setCountdown:base::Seconds(25)]);
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_OCMOCK_VERIFY(idle_timeout_confirmation_consumer_);
}

// Test that the presenter is called to stop showing the dialog after 30
// seconds.
TEST_F(IdleTimeoutConfirmationMediatorTest, TestExpiryCallsPresenter) {
  OCMExpect(
      [idle_timeout_confirmation_presenter_ stopPresentingAfterDialogExpired]);
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_OCMOCK_VERIFY(idle_timeout_confirmation_presenter_);
}
