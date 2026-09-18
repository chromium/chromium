// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expose TTCAudioEngineDelegate conformance for unit testing.
@interface TTCMediator (Testing) <TTCAudioEngineDelegate>
@end

namespace {

class TTCMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_audio_engine_ = OCMClassMock([TTCAudioEngine class]);
    mediator_ = [[TTCMediator alloc] initWithAudioEngine:mock_audio_engine_];
    mock_consumer_ = OCMProtocolMock(@protocol(TTCConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_audio_engine_ = nil;
    mock_consumer_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  id mock_audio_engine_ = nil;
  TTCMediator* mediator_ = nil;
  id mock_consumer_ = nil;
};

// Tests that attaching a consumer immediately triggers initial state
// hydration.
TEST_F(TTCMediatorTest, TestSetConsumerPushesInitialState) {
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  mediator_.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a voice session requests mic permission and updates the
// consumer to listening state when granted and recording starts successfully.
TEST_F(TTCMediatorTest, TestStartSessionSuccessUpdatesState) {
  mediator_.consumer = mock_consumer_;

  OCMStub([mock_audio_engine_
      requestMicrophonePermissionWithCompletion:([OCMArg
                                                    invokeBlockWithArgs:@YES,
                                                                        nil])]);
  OCMStub([mock_audio_engine_
      startRecordingWithCompletion:([OCMArg invokeBlockWithArgs:@YES,
                                                                [NSNull null],
                                                                nil])]);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kConnecting]);
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kListening]);

  [mediator_ startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a voice session updates to error state when microphone
// permission is denied.
TEST_F(TTCMediatorTest, TestStartSessionPermissionDenied) {
  mediator_.consumer = mock_consumer_;

  OCMStub([mock_audio_engine_
      requestMicrophonePermissionWithCompletion:([OCMArg
                                                    invokeBlockWithArgs:@NO,
                                                                        nil])]);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kConnecting]);
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kError]);
  OCMExpect([mock_consumer_ didEncounterError:@"Microphone permission denied"]);

  [mediator_ startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a voice session updates to error state when recording
// fails to start.
TEST_F(TTCMediatorTest, TestStartSessionRecordingFails) {
  mediator_.consumer = mock_consumer_;

  NSError* error = [NSError
      errorWithDomain:@"org.chromium.ttc.test"
                 code:-1
             userInfo:@{NSLocalizedDescriptionKey : @"Hardware init failure"}];

  OCMStub([mock_audio_engine_
      requestMicrophonePermissionWithCompletion:([OCMArg
                                                    invokeBlockWithArgs:@YES,
                                                                        nil])]);
  OCMStub([mock_audio_engine_
      startRecordingWithCompletion:([OCMArg
                                       invokeBlockWithArgs:@NO, error, nil])]);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kConnecting]);
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kError]);
  OCMExpect([mock_consumer_ didEncounterError:@"Hardware init failure"]);

  [mediator_ startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that stopping a voice session stops audio capture and resets consumer.
TEST_F(TTCMediatorTest, TestStopSessionUpdatesState) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_audio_engine_ stopRecording]);
  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  [mediator_ stopSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  EXPECT_OCMOCK_VERIFY(mock_audio_engine_);
}

// Tests that audio engine energy updates are forwarded to the consumer when
// listening.
TEST_F(TTCMediatorTest, TestAudioEngineDelegateUpdatesEnergy) {
  mediator_.consumer = mock_consumer_;

  OCMStub([mock_audio_engine_
      requestMicrophonePermissionWithCompletion:([OCMArg
                                                    invokeBlockWithArgs:@YES,
                                                                        nil])]);
  OCMStub([mock_audio_engine_
      startRecordingWithCompletion:([OCMArg invokeBlockWithArgs:@YES,
                                                                [NSNull null],
                                                                nil])]);
  [mediator_ startSession];

  OCMExpect([mock_consumer_ setMicEnergyLevel:0.85f]);

  [mediator_ audioEngine:mock_audio_engine_ didUpdateInputEnergy:0.85f];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio engine energy updates are ignored when not in listening
// state.
TEST_F(TTCMediatorTest, TestAudioEngineDelegateIgnoresEnergyWhenIdle) {
  mediator_.consumer = mock_consumer_;

  // Mediator is in kIdle state; verify no energy updates are forwarded.
  [[mock_consumer_ reject] setMicEnergyLevel:0.85f];

  [mediator_ audioEngine:mock_audio_engine_ didUpdateInputEnergy:0.85f];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that stopping a session while connecting discards delayed callbacks.
TEST_F(TTCMediatorTest, TestStopSessionWhileConnectingDiscardsCallbacks) {
  mediator_.consumer = mock_consumer_;

  __block void (^captured_permission_block)(BOOL) = nil;
  OCMStub([mock_audio_engine_
      requestMicrophonePermissionWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                            id block) {
        captured_permission_block = [block copy];
        return YES;
      }]]);

  [mediator_ startSession];

  // Stop session before permission resolves.
  [mediator_ stopSession];

  // Verify that subsequent permission grant does not transition to kListening.
  [[mock_consumer_ reject] setSessionState:TTCSessionState::kListening];

  if (captured_permission_block) {
    captured_permission_block(YES);
  }

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio engine errors transition the mediator to error state.
TEST_F(TTCMediatorTest, TestAudioEngineDelegateEncounterError) {
  mediator_.consumer = mock_consumer_;

  NSError* error = [NSError
      errorWithDomain:@"org.chromium.ttc.test"
                 code:-2
             userInfo:@{NSLocalizedDescriptionKey : @"Audio engine crashed"}];

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kError]);
  OCMExpect([mock_consumer_ didEncounterError:@"Audio engine crashed"]);

  [mediator_ audioEngine:mock_audio_engine_ didEncounterError:error];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that viewWillAppear rehydrates consumer state.
TEST_F(TTCMediatorTest, TestViewWillAppearHydratesConsumer) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  [mediator_ viewWillAppear];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling disconnect unhooks audio engine and clears the consumer.
TEST_F(TTCMediatorTest, TestDisconnectClearsConsumer) {
  mediator_.consumer = mock_consumer_;
  ASSERT_TRUE(mediator_.consumer != nil);

  OCMExpect([mock_audio_engine_ disconnect]);

  [mediator_ disconnect];

  EXPECT_TRUE(mediator_.consumer == nil);
  EXPECT_OCMOCK_VERIFY(mock_audio_engine_);
}

}  // namespace
