// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class TTCMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kTTCEnabled);
    TTCKeyedServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    ttc_service_ = TTCKeyedService::Get(profile_.get());
    mediator_ = [[TTCMediator alloc] initWithTTCService:ttc_service_];
    mock_consumer_ = OCMProtocolMock(@protocol(TTCConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    ttc_service_ = nullptr;
    mock_consumer_ = nil;
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TTCKeyedService> ttc_service_ = nullptr;
  TTCMediator* mediator_ = nil;
  id mock_consumer_ = nil;
};

// Tests that attaching a consumer pushes initial session state, 0.0 RMS energy,
// and default diagnostic states.
TEST_F(TTCMediatorTest, TestSetConsumerPushesInitialState) {
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  OCMExpect([mock_consumer_ setLoopbackEnabled:NO]);
  OCMExpect([mock_consumer_ setTestAudioPlaying:NO]);

  mediator_.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that attaching a consumer when a session is already live hydrates
// with listening state.
TEST_F(TTCMediatorTest, TestSetConsumerReflectsExistingActiveSession) {
  ttc_service_->StartSession();
  TTCSessionController* sessionController = ttc_service_->session_controller();
  ASSERT_TRUE(sessionController != nil);
  [sessionController onSessionInitialized];

  TTCMediator* liveMediator =
      [[TTCMediator alloc] initWithTTCService:ttc_service_];

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kListening]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  OCMExpect([mock_consumer_ setLoopbackEnabled:NO]);
  OCMExpect([mock_consumer_ setTestAudioPlaying:NO]);

  liveMediator.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  [liveMediator disconnect];
}

// Tests that starting a voice session transitions the consumer to connecting
// state via the session controller and starts the session on the service.
TEST_F(TTCMediatorTest, TestStartSessionDrivesTTCService) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kConnecting]);

  [mediator_ startSession];

  EXPECT_TRUE(ttc_service_->is_session_active());
  ASSERT_TRUE(ttc_service_->session_controller() != nil);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a session when the service is null transitions the
// consumer to error state.
TEST_F(TTCMediatorTest, TestStartSessionFailsWhenServiceUnavailable) {
  TTCMediator* nullServiceMediator =
      [[TTCMediator alloc] initWithTTCService:nullptr];
  nullServiceMediator.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kError]);
  OCMExpect([mock_consumer_ didEncounterError:@"TTC service is unavailable"]);

  [nullServiceMediator startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  [nullServiceMediator disconnect];
}

// Tests that stopping a voice session ends the active session on the service
// and resets the consumer state via session lifecycle completion.
TEST_F(TTCMediatorTest, TestStopSessionDrivesTTCService) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  ASSERT_TRUE(ttc_service_->is_session_active());

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  [mediator_ stopSession];

  EXPECT_FALSE(ttc_service_->is_session_active());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that lifecycle transitions dispatched by the active session controller
// update the consumer appropriately.
TEST_F(TTCMediatorTest, TestSessionLifecycleUpdatesConsumer) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  TTCSessionController* sessionController = ttc_service_->session_controller();
  ASSERT_TRUE(sessionController != nil);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kListening]);
  [sessionController onSessionInitialized];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  [sessionController stopSession];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio level updates from the session controller are forwarded to
// the consumer when the session is in listening state.
TEST_F(TTCMediatorTest, TestSessionAudioLevelUpdatesConsumerWhenListening) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  TTCSessionController* sessionController = ttc_service_->session_controller();
  ASSERT_TRUE(sessionController != nil);

  [sessionController onSessionInitialized];

  OCMExpect([mock_consumer_ setMicEnergyLevel:0.65f]);
  [sessionController userAudioLevelDidUpdate:0.65f];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio level updates from the session controller are ignored when
// the session is not in listening state.
TEST_F(TTCMediatorTest, TestSessionAudioLevelIgnoredWhenNotListening) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  TTCSessionController* sessionController = ttc_service_->session_controller();
  ASSERT_TRUE(sessionController != nil);

  // Still in kConnecting (initializing) state; audio updates should be ignored.
  [[mock_consumer_ reject] setMicEnergyLevel:0.65f];
  [sessionController userAudioLevelDidUpdate:0.65f];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that session errors dispatched by the session controller transition the
// consumer to error state and display the error description.
TEST_F(TTCMediatorTest, TestSessionErrorUpdatesConsumer) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  TTCSessionController* sessionController = ttc_service_->session_controller();
  ASSERT_TRUE(sessionController != nil);

  NSError* error =
      [NSError errorWithDomain:@"org.chromium.ttc.test"
                          code:-1
                      userInfo:@{
                        NSLocalizedDescriptionKey : @"Network connection lost"
                      }];

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kError]);
  OCMExpect([mock_consumer_ didEncounterError:@"Network connection lost"]);

  [sessionController failWithError:error];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that terminating the session directly on the service notifies the
// mediator and resets the consumer.
TEST_F(TTCMediatorTest, TestExternalSessionTerminationUpdatesConsumer) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  ASSERT_TRUE(ttc_service_->is_session_active());

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  ttc_service_->EndSession();

  EXPECT_FALSE(ttc_service_->is_session_active());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling disconnect detaches the consumer and observers while
// the underlying profile session continues running.
TEST_F(TTCMediatorTest, TestDisconnectDoesNotEndServiceSession) {
  mediator_.consumer = mock_consumer_;

  [mediator_ startSession];
  ASSERT_TRUE(ttc_service_->is_session_active());

  [mediator_ disconnect];

  // The session must persist independently of the UI mediator.
  EXPECT_TRUE(ttc_service_->is_session_active());
  EXPECT_TRUE(mediator_.consumer == nil);
}

// Tests that mutator diagnostic stubs update the consumer.
TEST_F(TTCMediatorTest, TestDiagnosticStubs) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setLoopbackEnabled:YES]);
  [mediator_ setLoopbackEnabled:YES];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_ setTestAudioPlaying:NO]);
  [mediator_ playTestAudio];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_ setTestAudioPlaying:NO]);
  [mediator_ stopTestAudio];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling viewWillAppear triggers complete hydration of consumer.
TEST_F(TTCMediatorTest, TestViewWillAppearHydratesConsumer) {
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  OCMExpect([mock_consumer_ setLoopbackEnabled:NO]);
  OCMExpect([mock_consumer_ setTestAudioPlaying:NO]);

  [mediator_ viewWillAppear];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

}  // namespace
