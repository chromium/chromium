// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class TTCMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    mediator_ = [[TTCMediator alloc] init];
    mock_consumer_ = OCMProtocolMock(@protocol(TTCConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_consumer_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TTCMediator* mediator_ = nil;
  id mock_consumer_ = nil;
};

// Tests that attaching a consumer pushes initial session state and 0.0 RMS
// energy.
TEST_F(TTCMediatorTest, TestSetConsumerPushesInitialState) {
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  mediator_.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a voice session updates the consumer to listening state.
TEST_F(TTCMediatorTest, TestStartSessionUpdatesState) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kListening]);

  [mediator_ startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that stopping a voice session resets the consumer to idle state.
TEST_F(TTCMediatorTest, TestStopSessionUpdatesState) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);

  [mediator_ stopSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling viewWillAppear triggers complete hydration of consumer.
TEST_F(TTCMediatorTest, TestViewWillAppearHydratesConsumer) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  [mediator_ viewWillAppear];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling disconnect unhooks and clears the consumer.
TEST_F(TTCMediatorTest, TestDisconnectClearsConsumer) {
  mediator_.consumer = mock_consumer_;
  ASSERT_TRUE(mediator_.consumer != nil);

  [mediator_ disconnect];

  EXPECT_TRUE(mediator_.consumer == nil);
}
