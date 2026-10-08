// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/run_until.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
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

// Fake observer conforming to TTCSessionControllerObserver for unit testing.
@interface FakeTTCSessionControllerObserver
    : NSObject <TTCSessionControllerObserver>

@property(nonatomic, assign) NSInteger lifecycleChangeCount;
@property(nonatomic, assign) TTCSessionLifecycle lastLifecycle;
@property(nonatomic, assign) NSInteger audioLevelUpdateCount;
@property(nonatomic, assign) float lastAudioLevel;
@property(nonatomic, assign) NSInteger errorCount;
@property(nonatomic, strong) NSError* lastError;

@end

@implementation FakeTTCSessionControllerObserver

- (void)sessionController:(TTCSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle {
  _lifecycleChangeCount++;
  _lastLifecycle = lifecycle;
}

- (void)sessionController:(TTCSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel {
  _audioLevelUpdateCount++;
  _lastAudioLevel = audioLevel;
}

- (void)sessionController:(TTCSessionController*)controller
         didFailWithError:(NSError*)error {
  _errorCount++;
  _lastError = error;
}

@end

class TTCSessionControllerTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that a controller initializes in kInitializing state.
TEST_F(TTCSessionControllerTest, TestInitialLifecycleIsInitializing) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  ASSERT_TRUE(controller != nil);
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kInitializing);
  [controller disconnect];
}

// Tests that onSessionInitialized transitions the lifecycle to kLive and
// notifies observers.
TEST_F(TTCSessionControllerTest, TestSessionInitializedTransitionsToLive) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);

  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycleChangeCount, 1);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kLive);
  [controller disconnect];
}

// Tests that stopSession transitions to kFinished and disconnects.
TEST_F(TTCSessionControllerTest, TestStopSessionTransitionsToFinished) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);

  [controller stopSession];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycleChangeCount, 2);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);
}

// Tests that disconnect transitions lifecycle to kFinished and notifies
// observers.
TEST_F(TTCSessionControllerTest, TestObserverNotifiedOnDisconnect) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);

  [controller disconnect];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycleChangeCount, 1);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);
}

// Tests that UIApplicationDidEnterBackgroundNotification stops the session,
// moves lifecycle to kFinished, and notifies observers.
TEST_F(TTCSessionControllerTest, TestBackgroundNotificationStopsSession) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];

  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);

  [controller disconnect];
}

// Tests that userAudioLevelDidUpdate forwards the level to observers.
TEST_F(TTCSessionControllerTest, TestAudioLevelUpdateForwardedToObserver) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller userAudioLevelDidUpdate:0.65f];
  EXPECT_EQ(observer.audioLevelUpdateCount, 1);
  EXPECT_FLOAT_EQ(observer.lastAudioLevel, 0.65f);
  [controller disconnect];
}

// Tests that failWithError forwards the error to observers.
TEST_F(TTCSessionControllerTest, TestFailWithErrorForwardedToObserver) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  NSError* error = [NSError errorWithDomain:@"org.chromium.ttc"
                                       code:42
                                   userInfo:nil];
  [controller failWithError:error];
  EXPECT_EQ(observer.errorCount, 1);
  EXPECT_EQ(observer.lastError, error);
  [controller disconnect];
}

// Tests that removing an observer stops notifications.
TEST_F(TTCSessionControllerTest, TestRemoveObserverStopsNotifications) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];
  [controller removeObserver:observer];

  [controller onSessionInitialized];
  [controller userAudioLevelDidUpdate:0.5f];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);
  EXPECT_EQ(observer.audioLevelUpdateCount, 0);
  [controller disconnect];
}

// Tests that disconnect cleanly unregisters background notifications.
TEST_F(TTCSessionControllerTest,
       TestDisconnectCleanlyUnregistersBackgroundNotification) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  [controller startSession];
  [controller disconnect];

  // Posting background notification after disconnect should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
}

// Tests that adding or removing a nil observer is a safe no-op and does not
// crash.
TEST_F(TTCSessionControllerTest, TestAddObserverNilDoesNotCrash) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  [controller addObserver:nil];
  [controller removeObserver:nil];
  [controller disconnect];
}

// Tests that onSessionInitialized does not resurrect a session that has already
// finished.
TEST_F(TTCSessionControllerTest,
       TestLateOnSessionInitializedDoesNotResurrectFinishedSession) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller stopSession];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);

  // Late initialization after session termination must not resurrect to kLive.
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);

  [controller disconnect];
}

// Tests that audio level updates from background threads are safely delivered
// on the main thread.
TEST_F(TTCSessionControllerTest, TestAudioLevelDispatchedFromBackgroundThread) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                 ^{
                   [controller userAudioLevelDidUpdate:0.8f];
                 });

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.audioLevelUpdateCount == 1; }));
  EXPECT_EQ(observer.audioLevelUpdateCount, 1);
  EXPECT_FLOAT_EQ(observer.lastAudioLevel, 0.8f);

  [controller disconnect];
}

// Tests that TTCSessionController initializes with an injected conversation and
// assigns itself as the delegate.
TEST_F(TTCSessionControllerTest, TestCustomConversationInjection) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];

  EXPECT_EQ(controller.conversation, conversation);
  EXPECT_EQ(conversation.delegate, (id<TTCConversationDelegate>)controller);
  [controller disconnect];
}

// Tests that startSession starts the underlying conversation and capture.
TEST_F(TTCSessionControllerTest, TestStartSessionDrivesConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];

  EXPECT_FALSE(fake_audio.isCapturing);
  [controller startSession];
  EXPECT_TRUE(fake_audio.isCapturing);

  [controller disconnect];
}

// Tests that stopSession stops the underlying conversation and capture.
TEST_F(TTCSessionControllerTest, TestStopSessionDrivesConversationStop) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];

  [controller startSession];
  EXPECT_TRUE(fake_audio.isCapturing);

  [controller stopSession];
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_TRUE(fake_audio.didStopPlayback);

  [controller disconnect];
}

// Tests that conversation audio energy updates are forwarded to session
// observers.
TEST_F(TTCSessionControllerTest, TestConversationEnergyForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];

  // Conversation delegate invokes didUpdateAudioEnergy.
  [(id<TTCConversationDelegate>)controller conversation:conversation
                                   didUpdateAudioEnergy:0.75f];

  EXPECT_EQ(observer.audioLevelUpdateCount, 1);
  EXPECT_FLOAT_EQ(observer.lastAudioLevel, 0.75f);

  [controller disconnect];
}

// Tests that conversation errors are forwarded to session observers.
TEST_F(TTCSessionControllerTest, TestConversationErrorForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  NSError* error = [NSError errorWithDomain:@"TestConvError"
                                       code:-42
                                   userInfo:nil];
  [(id<TTCConversationDelegate>)controller conversation:conversation
                                      didEncounterError:error];

  EXPECT_EQ(observer.errorCount, 1);
  EXPECT_NSEQ(observer.lastError, error);

  [controller disconnect];
}

// Tests that TTCSessionController disconnect calls disconnect on the underlying
// conversation.
TEST_F(TTCSessionControllerTest, TestDisconnectCleansUpConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];

  [controller startSession];
  EXPECT_TRUE(fake_audio.isCapturing);

  [controller disconnect];
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_EQ(conversation.delegate, nil);
  EXPECT_EQ(fake_audio.delegate, nil);
}

// Tests that conversationDidInitialize transitions the lifecycle to kLive and
// notifies observers.
TEST_F(TTCSessionControllerTest,
       TestConversationDidInitializeTransitionsToLive) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  TTCConversation* conversation =
      [[TTCConversation alloc] initWithAudioController:fake_audio backend:nil];
  TTCSessionController* controller =
      [[TTCSessionController alloc] initWithConversation:conversation];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kInitializing);
  EXPECT_EQ(observer.lifecycleChangeCount, 0);

  [(id<TTCConversationDelegate>)controller
      conversationDidInitialize:conversation];

  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycleChangeCount, 1);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kLive);

  [controller disconnect];
}
