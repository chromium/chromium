// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_voice_plate_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class TtcVoicePlateMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kTTCEnabled);
    TTCKeyedServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    ttc_service_ = TTCKeyedServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[TTCVoicePlateMediator alloc] initWithTTCService:ttc_service_];
    mock_consumer_ = OCMProtocolMock(@protocol(TTCVoicePlateConsumer));
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
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TTCKeyedService> ttc_service_ = nullptr;
  TTCVoicePlateMediator* mediator_ = nil;
  id<TTCVoicePlateConsumer> mock_consumer_ = nil;
};

// Tests that setting the consumer pushes the initial idle state and zero
// energy.
TEST_F(TtcVoicePlateMediatorTest, SetConsumerPushesInitialState) {
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicMuted:NO]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  mediator_.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that attaching a consumer when a session is already live hydrates
// with listening state.
TEST_F(TtcVoicePlateMediatorTest, SetConsumerReflectsExistingActiveSession) {
  ttc_service_->StartSession();
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  session_controller->OnConversationInitialized();

  TTCVoicePlateMediator* live_mediator =
      [[TTCVoicePlateMediator alloc] initWithTTCService:ttc_service_];

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kListening]);
  OCMExpect([mock_consumer_ setMicMuted:NO]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  live_mediator.consumer = mock_consumer_;

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  [live_mediator disconnect];
}

// Tests that starting a voice session transitions the consumer to connecting
// state and starts the session on the service.
TEST_F(TtcVoicePlateMediatorTest, StartSessionDrivesTTCService) {
  mediator_.consumer = mock_consumer_;
  ASSERT_FALSE(ttc_service_->is_session_active());
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kConnecting]);

  [mediator_ startSession];

  EXPECT_TRUE(ttc_service_->is_session_active());
  ASSERT_NE(ttc_service_->session_controller(), nullptr);
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that stopping a voice session ends the active session on the service
// and resets the consumer to idle state with zero energy.
TEST_F(TtcVoicePlateMediatorTest, StopSessionDrivesTTCService) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  ASSERT_TRUE(ttc_service_->is_session_active());
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  [mediator_ stopSession];

  EXPECT_FALSE(ttc_service_->is_session_active());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that `startSession` fails gracefully when the feature is disabled.
TEST_F(TtcVoicePlateMediatorTest, StartSessionFailsWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kTTCEnabled);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  mediator_.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kError]);
  OCMExpect([mock_consumer_ showErrorMessage:@"TTC service is unavailable"]);

  [mediator_ startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  EXPECT_FALSE(ttc_service_->is_session_active());
}

// Tests that starting a session when the service is null transitions the
// consumer to error state.
TEST_F(TtcVoicePlateMediatorTest, StartSessionFailsWhenServiceNull) {
  TTCVoicePlateMediator* null_service_mediator =
      [[TTCVoicePlateMediator alloc] initWithTTCService:nullptr];
  null_service_mediator.consumer = mock_consumer_;

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kError]);
  OCMExpect([mock_consumer_ showErrorMessage:@"TTC service is unavailable"]);

  [null_service_mediator startSession];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
  [null_service_mediator disconnect];
}

// Tests that starting a new session succeeds after the previous session
// controller finishes.
TEST_F(TtcVoicePlateMediatorTest,
       StartSessionSucceedsAfterPreviousControllerFinishes) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  ASSERT_NE(ttc_service_->session_controller(), nullptr);
  ttc_service_->session_controller()->StopSession();
  ASSERT_FALSE(ttc_service_->is_session_active());

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kConnecting]);

  [mediator_ startSession];

  EXPECT_TRUE(ttc_service_->is_session_active());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that starting a session directly on the service transitions the
// consumer to connecting state via the service state callback.
TEST_F(TtcVoicePlateMediatorTest,
       ExternalServiceSessionStartTransitionsConsumerToConnecting) {
  mediator_.consumer = mock_consumer_;
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kConnecting]);

  ttc_service_->StartSession();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that ending a session directly on the service resets the consumer to
// idle state with zero energy via the service state callback.
TEST_F(TtcVoicePlateMediatorTest,
       ExternalServiceSessionEndResetsConsumerToIdle) {
  mediator_.consumer = mock_consumer_;
  ttc_service_->StartSession();
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  ttc_service_->EndSession();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that conversation initialization transitions the consumer to listening
// state.
TEST_F(TtcVoicePlateMediatorTest,
       ConversationInitializedTransitionsConsumerToListening) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kListening]);

  session_controller->OnConversationInitialized();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that stopping the session controller resets the consumer to idle state
// with zero energy.
TEST_F(TtcVoicePlateMediatorTest, SessionStoppedResetsConsumerToIdle) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  session_controller->OnConversationInitialized();
  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);

  session_controller->StopSession();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio level updates are ignored when the session is not in
// listening state.
TEST_F(TtcVoicePlateMediatorTest, AudioLevelUpdatesIgnoredWhenNotListening) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  OCMReject([mock_consumer_ setMicEnergyLevel:0.65f]);

  session_controller->OnAudioEnergyUpdated(0.65f);

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that audio level updates are forwarded to the consumer when the session
// is in listening state.
TEST_F(TtcVoicePlateMediatorTest, AudioLevelUpdatesForwardedWhenListening) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  session_controller->OnConversationInitialized();
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.65f]);

  session_controller->OnAudioEnergyUpdated(0.65f);

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that session error dispatches `showErrorMessage:` to the consumer.
TEST_F(TtcVoicePlateMediatorTest, SessionErrorDispatchesErrorMessage) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];

  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);

  OCMExpect([mock_consumer_ setSessionState:TTCSessionUIState::kError]);
  OCMExpect([mock_consumer_ setMicMuted:NO]);
  OCMExpect([mock_consumer_ showErrorMessage:@"InternalBackendError"]);
  OCMExpect([mock_consumer_ setMicEnergyLevel:0.0f]);
  OCMReject([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);

  session_controller->OnConversationError(
      ttc::ErrorCode::kInternalBackendError);

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that `toggleMicMuted` mutes and unmutes the active session controller
// and updates the consumer.
TEST_F(TtcVoicePlateMediatorTest, ToggleMicMutedTogglesActiveSessionMute) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  session_controller->OnConversationInitialized();

  OCMExpect([mock_consumer_ setMicMuted:YES]);
  [mediator_ toggleMicMuted];
  EXPECT_TRUE(session_controller->IsMicMuted());

  OCMExpect([mock_consumer_ setMicMuted:NO]);
  [mediator_ toggleMicMuted];
  EXPECT_FALSE(session_controller->IsMicMuted());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling `disconnect` preserves the active profile session.
TEST_F(TtcVoicePlateMediatorTest, DisconnectPreservesActiveSession) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];

  [mediator_ disconnect];

  EXPECT_TRUE(ttc_service_->is_session_active());
}

// Tests that calling `disconnect` detaches the consumer and stops forwarding
// session controller and service events.
TEST_F(TtcVoicePlateMediatorTest, DisconnectStopsForwardingEvents) {
  mediator_.consumer = mock_consumer_;
  [mediator_ startSession];
  TtcSessionController* session_controller = ttc_service_->session_controller();
  ASSERT_NE(session_controller, nullptr);
  OCMReject([mock_consumer_ setSessionState:TTCSessionUIState::kListening]);
  OCMReject([mock_consumer_ setMicEnergyLevel:0.5f]);
  OCMReject([mock_consumer_ setSessionState:TTCSessionUIState::kIdle]);

  [mediator_ disconnect];
  session_controller->OnConversationInitialized();
  session_controller->OnAudioEnergyUpdated(0.5f);
  ttc_service_->EndSession();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

}  // namespace
