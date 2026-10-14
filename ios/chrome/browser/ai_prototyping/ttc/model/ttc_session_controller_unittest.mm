// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/test_utils.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#pragma mark - Fake Audio Controller

@interface FakeTTCSessionAudioController : NSObject <TTCAudioController>

@property(nonatomic, weak) id<TTCAudioControllerDelegate> delegate;
@property(nonatomic, assign, getter=isCapturing) BOOL capturing;
@property(nonatomic, assign, getter=isPlaying) BOOL playing;
@property(nonatomic, assign, getter=isLoopbackEnabled) BOOL loopbackEnabled;
@property(nonatomic, assign, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;
@property(nonatomic, assign) BOOL didStopCapture;
@property(nonatomic, assign) BOOL didStopPlayback;

@end

@implementation FakeTTCSessionAudioController

- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion {
  self.capturing = YES;
  if (completion) {
    completion(YES, nil);
  }
}

- (void)stopCapture {
  self.capturing = NO;
  self.didStopCapture = YES;
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  self.playing = YES;
}

- (void)clearPlaybackQueue {
}

- (void)stopPlayback {
  self.playing = NO;
  self.didStopPlayback = YES;
}

- (void)playTestTone {
}

- (void)stopTestTone {
}

- (void)disconnect {
}

@end

#pragma mark - Fake Session Observer

namespace {

// Fake observer implementing `TtcSessionControllerObserver` for unit testing.
class FakeTtcSessionControllerObserver : public TtcSessionControllerObserver {
 public:
  FakeTtcSessionControllerObserver() = default;
  ~FakeTtcSessionControllerObserver() override = default;

  void OnSessionLifecycleChanged(TtcSessionController* controller,
                                 TTCSessionLifecycle lifecycle) override {
    lifecycle_change_count_++;
    last_lifecycle_ = lifecycle;
  }

  void OnUserAudioLevelUpdated(TtcSessionController* controller,
                               float audio_level) override {
    audio_level_update_count_++;
    last_audio_level_ = audio_level;
  }

  void OnSessionError(TtcSessionController* controller,
                      ttc::ErrorCode error) override {
    error_count_++;
    last_error_ = error;
  }

  int lifecycle_change_count() const { return lifecycle_change_count_; }
  TTCSessionLifecycle last_lifecycle() const { return last_lifecycle_; }
  int audio_level_update_count() const { return audio_level_update_count_; }
  float last_audio_level() const { return last_audio_level_; }
  int error_count() const { return error_count_; }
  ttc::ErrorCode last_error() const { return last_error_; }

 private:
  int lifecycle_change_count_ = 0;
  TTCSessionLifecycle last_lifecycle_ = TTCSessionLifecycle::kInitializing;
  int audio_level_update_count_ = 0;
  float last_audio_level_ = 0.0f;
  int error_count_ = 0;
  ttc::ErrorCode last_error_ = ttc::ErrorCode::kUnknown;
};

class TtcSessionControllerTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that a controller initializes in `kInitializing` state.
TEST_F(TtcSessionControllerTest, TestInitialLifecycleIsInitializing) {
  TtcSessionController controller;
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kInitializing);
  controller.Disconnect();
}

// Tests that `OnSessionInitialized` transitions the lifecycle to `kLive` and
// notifies observers.
TEST_F(TtcSessionControllerTest, TestSessionInitializedTransitionsToLive) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  controller.OnSessionInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kLive);
  controller.Disconnect();
}

// Tests that `StopSession` transitions the lifecycle to `kFinished`.
TEST_F(TtcSessionControllerTest, TestStopSessionTransitionsToFinished) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnSessionInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  controller.StopSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycle_change_count(), 2);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
  controller.Disconnect();
}

// Tests that `Disconnect` transitions lifecycle to `kFinished`, notifies
// observers, and invalidates weak pointers.
TEST_F(TtcSessionControllerTest, TestObserverNotifiedOnDisconnect) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);
  base::WeakPtr<TtcSessionController> weak_controller = controller.GetWeakPtr();
  EXPECT_TRUE(weak_controller);

  controller.StartSession();
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  controller.Disconnect();
  EXPECT_FALSE(weak_controller);
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
}

// Tests that `UIApplicationDidEnterBackgroundNotification` stops the session,
// moves lifecycle to `kFinished`, and notifies observers.
TEST_F(TtcSessionControllerTest, TestBackgroundNotificationStopsSession) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnSessionInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);

  controller.Disconnect();
}

// Tests that `UserAudioLevelDidUpdate` forwards the level to observers.
TEST_F(TtcSessionControllerTest, TestAudioLevelUpdateForwardedToObserver) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.UserAudioLevelDidUpdate(0.65f);
  EXPECT_EQ(observer.audio_level_update_count(), 1);
  EXPECT_FLOAT_EQ(observer.last_audio_level(), 0.65f);
  controller.Disconnect();
}

// Tests that `FailWithError` forwards the error to observers.
TEST_F(TtcSessionControllerTest, TestFailWithErrorForwardedToObserver) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.FailWithError(ttc::ErrorCode::kInternalBackendError);
  EXPECT_EQ(observer.error_count(), 1);
  EXPECT_EQ(observer.last_error(), ttc::ErrorCode::kInternalBackendError);
  controller.Disconnect();
}

// Tests that removing an observer stops notifications.
TEST_F(TtcSessionControllerTest, TestRemoveObserverStopsNotifications) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);
  controller.RemoveObserver(&observer);

  controller.OnSessionInitialized();
  controller.UserAudioLevelDidUpdate(0.5f);
  EXPECT_EQ(observer.lifecycle_change_count(), 0);
  EXPECT_EQ(observer.audio_level_update_count(), 0);
  controller.Disconnect();
}

// Tests that `Disconnect` cleanly unregisters background notifications.
TEST_F(TtcSessionControllerTest,
       TestDisconnectCleanlyUnregistersBackgroundNotification) {
  TtcSessionController controller;
  controller.StartSession();
  controller.Disconnect();

  // Posting background notification after disconnect should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
}

// Tests that adding or removing a `nullptr` observer is a safe no-op and does
// not crash.
TEST_F(TtcSessionControllerTest, TestAddObserverNullDoesNotCrash) {
  TtcSessionController controller;
  controller.AddObserver(nullptr);
  controller.RemoveObserver(nullptr);
  controller.Disconnect();
}

// Tests that `OnSessionInitialized` does not resurrect a session that has
// already finished.
TEST_F(TtcSessionControllerTest,
       TestLateOnSessionInitializedDoesNotResurrectFinishedSession) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.StopSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);

  // Late initialization after session termination must not resurrect to
  // `kLive`.
  controller.OnSessionInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);

  controller.Disconnect();
}

// Tests that `TtcSessionController` initializes with an injected conversation
// and assigns itself as delegate.
TEST_F(TtcSessionControllerTest, TestCustomConversationInjection) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));

  EXPECT_EQ(controller.conversation(), conversation_ptr);
  EXPECT_EQ(conversation_ptr->delegate(), &controller);
  controller.Disconnect();
}

// Tests that constructing `TtcSessionController` with a `ttc::TtcBackend`
// creates a conversation backed by that backend and assigns itself as delegate.
TEST_F(TtcSessionControllerTest, TestInitWithBackend) {
  auto backend = std::make_unique<testing::NiceMock<ttc::MockTtcBackend>>();
  ttc::TtcBackend* backend_ptr = backend.get();
  TtcSessionController controller(std::move(backend));

  ASSERT_NE(controller.conversation(), nullptr);
  EXPECT_EQ(controller.conversation()->backend(), backend_ptr);
  EXPECT_EQ(controller.conversation()->delegate(), &controller);
  controller.Disconnect();
}

// Tests that `StartSession` starts the underlying conversation and capture.
TEST_F(TtcSessionControllerTest, TestStartSessionDrivesConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcSessionController controller(std::move(conversation));

  EXPECT_FALSE(fake_audio.isCapturing);
  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.Disconnect();
}

// Tests that `StopSession` stops the underlying conversation and capture.
TEST_F(TtcSessionControllerTest, TestStopSessionDrivesConversationStop) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcSessionController controller(std::move(conversation));

  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.StopSession();
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_TRUE(fake_audio.didStopPlayback);

  controller.Disconnect();
}

// Tests that conversation audio energy updates are forwarded to session
// observers.
TEST_F(TtcSessionControllerTest, TestConversationEnergyForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  ASSERT_NE(conversation_ptr->delegate(), nullptr);

  conversation_ptr->delegate()->OnAudioEnergyUpdated(0.75f);

  EXPECT_EQ(observer.audio_level_update_count(), 1);
  EXPECT_FLOAT_EQ(observer.last_audio_level(), 0.75f);

  controller.Disconnect();
}

// Tests that conversation errors are forwarded to session observers and stop
// the session when fatal.
TEST_F(TtcSessionControllerTest, TestConversationErrorForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  conversation_ptr->delegate()->OnConversationError(
      ttc::ErrorCode::kInternalBackendError);

  EXPECT_EQ(observer.error_count(), 1);
  EXPECT_EQ(observer.last_error(), ttc::ErrorCode::kInternalBackendError);
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_TRUE(fake_audio.didStopCapture);

  // Subsequent fatal errors must be ignored once a fatal error is reported.
  conversation_ptr->delegate()->OnConversationError(
      ttc::ErrorCode::kSessionExpired);
  EXPECT_EQ(observer.error_count(), 1);

  controller.Disconnect();
}

// Tests that `OnConversationClosed` stops the session and transitions lifecycle
// to `kFinished`.
TEST_F(TtcSessionControllerTest, TestConversationDidCloseStopsSession) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnSessionInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  conversation_ptr->delegate()->OnConversationClosed();

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_TRUE(fake_audio.didStopCapture);

  controller.Disconnect();
}

// Tests that `TtcSessionController` `Disconnect` calls `Disconnect` on the
// underlying conversation.
TEST_F(TtcSessionControllerTest, TestDisconnectCleansUpConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));

  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.Disconnect();
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_EQ(conversation_ptr->delegate(), nullptr);
  EXPECT_EQ(fake_audio.delegate, nil);
}

// Tests that `OnConversationInitialized` transitions the lifecycle to `kLive`
// and notifies observers.
TEST_F(TtcSessionControllerTest,
       TestConversationDidInitializeTransitionsToLive) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kInitializing);
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  conversation_ptr->delegate()->OnConversationInitialized();

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kLive);

  controller.Disconnect();
}

}  // namespace
