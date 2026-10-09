// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager_delegate.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_audio_engine_protocol.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expose internal delegate conformance and properties for unit testing.
@interface TTCAudioSessionController (Testing) <TTCAudioEngineDelegate,
                                                TTCAudioSessionManagerDelegate>
@property(nonatomic, readonly) id<TTCAudioEngineProtocol> audioEngine;
@property(nonatomic, readonly) TTCAudioSessionManager* sessionManager;
@end

#pragma mark - Fake TTCAudioEngineProtocol

@interface FakeTTCAudioEngine : NSObject <TTCAudioEngineProtocol>

@property(nonatomic, weak) id<TTCAudioEngineDelegate> delegate;
@property(nonatomic, assign, getter=isStarted) BOOL started;
@property(nonatomic, assign) TTCAudioAECMode aecMode;
@property(nonatomic, assign, getter=isCapturing) BOOL capturing;
@property(nonatomic, assign) float inputAudioLevel;
@property(nonatomic, assign, getter=isPlaying) BOOL playing;

@property(nonatomic, assign) BOOL shouldFailStart;
@property(nonatomic, strong) NSError* startError;
@property(nonatomic, assign) BOOL shouldFailStartCapture;
@property(nonatomic, assign) BOOL deferStartCompletion;
@property(nonatomic, copy) void (^pendingStartCompletion)
    (BOOL success, NSError* error);

@property(nonatomic, assign) NSUInteger startCallCount;
@property(nonatomic, assign) NSUInteger stopCallCount;
@property(nonatomic, assign) NSUInteger startCaptureCallCount;
@property(nonatomic, assign) NSUInteger stopCaptureCallCount;
@property(nonatomic, assign) NSUInteger stopPlaybackCallCount;
@property(nonatomic, assign) BOOL didDisconnect;
@property(nonatomic, strong) NSMutableArray<NSData*>* scheduledChunks;

@end

@implementation FakeTTCAudioEngine

- (instancetype)init {
  self = [super init];
  if (self) {
    _scheduledChunks = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)startWithCompletion:(void (^)(BOOL success, NSError* error))completion {
  self.startCallCount++;
  if (self.deferStartCompletion) {
    self.pendingStartCompletion = completion;
    return;
  }
  if (self.shouldFailStart) {
    self.started = NO;
    if (completion) {
      completion(NO, self.startError);
    }
    return;
  }
  self.started = YES;
  if (completion) {
    completion(YES, nil);
  }
}

- (void)stopWithCompletion:(void (^)(BOOL success, NSError* error))completion {
  self.stopCallCount++;
  self.capturing = NO;
  self.playing = NO;
  self.started = NO;
  if (completion) {
    completion(YES, nil);
  }
}

- (void)disconnect {
  self.didDisconnect = YES;
  self.capturing = NO;
  self.playing = NO;
  self.started = NO;
}

- (BOOL)startCapture {
  self.startCaptureCallCount++;
  if (!self.isStarted || self.shouldFailStartCapture) {
    return NO;
  }
  self.capturing = YES;
  return YES;
}

- (void)stopCapture {
  self.stopCaptureCallCount++;
  self.capturing = NO;
}

- (void)schedulePlaybackData:(NSData*)pcm24kData {
  if (!self.isStarted || pcm24kData.length == 0) {
    return;
  }
  [self.scheduledChunks addObject:pcm24kData];
  self.playing = YES;
}

- (void)notifyEndOfPlaybackData {
}

- (void)stopPlayback {
  self.stopPlaybackCallCount++;
  [self.scheduledChunks removeAllObjects];
  self.playing = NO;
}

@end

#pragma mark - Fake TTCAudioSessionManager

@interface FakeTTCAudioSessionManager : TTCAudioSessionManager
@property(nonatomic, assign) BOOL mockHasHardwareAEC;
@property(nonatomic, assign) BOOL mockOutputRoutedToSpeaker;
@property(nonatomic, strong) NSError* mockConfigureError;
@property(nonatomic, assign) NSUInteger configureCallCount;
@property(nonatomic, assign) NSUInteger restoreCallCount;
@property(nonatomic, assign) BOOL didDisconnect;
@end

@implementation FakeTTCAudioSessionManager

- (BOOL)hasHardwareAEC {
  return self.mockHasHardwareAEC;
}

- (BOOL)isOutputRoutedToSpeaker {
  return self.mockOutputRoutedToSpeaker;
}

- (NSError*)configureAudioSession {
  self.configureCallCount++;
  return self.mockConfigureError;
}

- (void)configureAudioSessionWithCompletion:
    (void (^)(NSError* error))completion {
  self.configureCallCount++;
  if (completion) {
    completion(self.mockConfigureError);
  }
}

- (void)restoreAudioSessionCategory {
  self.restoreCallCount++;
}

- (void)disconnect {
  self.didDisconnect = YES;
  self.delegate = nil;
}

@end

#pragma mark - Fake TTCAudioControllerDelegate

@interface FakeTTCAudioControllerDelegate
    : NSObject <TTCAudioControllerDelegate>
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, assign) BOOL didStartCapture;
@property(nonatomic, assign) BOOL didStopCapture;
@property(nonatomic, assign) BOOL didStartPlayback;
@property(nonatomic, assign) BOOL didStopPlayback;
@property(nonatomic, strong) NSError* lastError;
@property(nonatomic, strong) NSData* lastCapturedChunk;
@property(nonatomic, assign) BOOL didChangeRoute;
@end

@implementation FakeTTCAudioControllerDelegate

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  self.lastEnergy = energy;
}

- (void)audioControllerDidStartCapture:(id<TTCAudioController>)controller {
  self.didStartCapture = YES;
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  self.didStopCapture = YES;
}

- (void)audioControllerDidStartPlayback:(id<TTCAudioController>)controller {
  self.didStartPlayback = YES;
}

- (void)audioControllerDidStopPlayback:(id<TTCAudioController>)controller {
  self.didStopPlayback = YES;
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  self.lastError = error;
}

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  self.lastCapturedChunk = pcmData;
}

- (void)audioControllerDidChangeRoute:(id<TTCAudioController>)controller {
  self.didChangeRoute = YES;
}

@end

namespace {

class TTCAudioSessionControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    fake_engine_ = [[FakeTTCAudioEngine alloc] init];
    fake_session_manager_ = [[FakeTTCAudioSessionManager alloc] init];
    delegate_ = [[FakeTTCAudioControllerDelegate alloc] init];
    controller_ = [[TTCAudioSessionController alloc]
        initWithAudioEngine:fake_engine_
             sessionManager:fake_session_manager_];
    controller_.delegate = delegate_;
  }

  void TearDown() override {
    [controller_ disconnect];
    controller_ = nil;
    fake_engine_ = nil;
    fake_session_manager_ = nil;
    delegate_ = nil;
    if (mock_audio_app_) {
      [mock_audio_app_ stopMocking];
      mock_audio_app_ = nil;
    }
    PlatformTest::TearDown();
  }

  void SetUpMockAudioApp(AVAudioApplicationRecordPermission permission) {
    mock_audio_app_ = OCMClassMock([AVAudioApplication class]);
    OCMStub(ClassMethod([mock_audio_app_ sharedInstance]))
        .andReturn(mock_audio_app_);
    OCMStub([mock_audio_app_ recordPermission]).andReturn(permission);
  }

  web::WebTaskEnvironment task_environment_;
  FakeTTCAudioEngine* fake_engine_ = nil;
  FakeTTCAudioSessionManager* fake_session_manager_ = nil;
  FakeTTCAudioControllerDelegate* delegate_ = nil;
  TTCAudioSessionController* controller_ = nil;
  id mock_audio_app_ = nil;
};

// Tests that default initialization instantiates `TTCAudioEngine` and
// designated initialization accepts a custom `id<TTCAudioEngineProtocol>`.
TEST_F(TTCAudioSessionControllerTest,
       TestDefaultAndCustomEngineInitialization) {
  TTCAudioSessionController* default_controller =
      [[TTCAudioSessionController alloc] init];
  EXPECT_TRUE(
      [default_controller.audioEngine isKindOfClass:[TTCAudioEngine class]]);
  [default_controller disconnect];

  FakeTTCAudioEngine* custom_engine = [[FakeTTCAudioEngine alloc] init];
  TTCAudioSessionController* custom_controller =
      [[TTCAudioSessionController alloc] initWithAudioEngine:custom_engine
                                              sessionManager:nil];
  EXPECT_NSEQ(custom_controller.audioEngine, custom_engine);
  [custom_controller disconnect];
}

// Tests that startCaptureWithCompletion fails with permission denied when
// microphone permission is denied.
TEST_F(TTCAudioSessionControllerTest, TestStartCapturePermissionDenied) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionDenied);

  base::test::TestFuture<BOOL, NSError*> future;
  [controller_
      startCaptureWithCompletion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_FALSE(success);
  ASSERT_NE(error, nil);
  EXPECT_NSEQ(error.domain, kTTCAudioSessionControllerErrorDomain);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(
                TTCAudioSessionControllerErrorCode::kPermissionDenied));
  EXPECT_FALSE(controller_.isCapturing);
}

// Tests that startCaptureWithCompletion requests permission when undetermined
// and cancels when permission is denied by the user.
TEST_F(TTCAudioSessionControllerTest,
       TestStartCapturePermissionUndeterminedDenied) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionUndetermined);
  OCMStub(ClassMethod([mock_audio_app_
              requestRecordPermissionWithCompletionHandler:[OCMArg any]]))
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained void (^handler)(BOOL granted) = nil;
        [invocation getArgument:&handler atIndex:2];
        if (handler) {
          handler(NO);
        }
      });

  base::test::TestFuture<BOOL, NSError*> future;
  [controller_
      startCaptureWithCompletion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_FALSE(success);
  ASSERT_NE(error, nil);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(
                TTCAudioSessionControllerErrorCode::kPermissionDenied));
  EXPECT_FALSE(controller_.isCapturing);
}

// Tests that startCaptureWithCompletion configures the session, starts the
// engine, and begins capture when permission is granted.
TEST_F(TTCAudioSessionControllerTest, TestStartCapturePermissionGranted) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionGranted);

  base::test::TestFuture<BOOL, NSError*> future;
  [controller_
      startCaptureWithCompletion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_NSEQ(error, nil);
  EXPECT_TRUE(controller_.isCapturing);
  EXPECT_TRUE(delegate_.didStartCapture);
  EXPECT_EQ(fake_session_manager_.configureCallCount, 1u);
  EXPECT_EQ(fake_engine_.startCallCount, 1u);
  EXPECT_EQ(fake_engine_.startCaptureCallCount, 1u);
}

// Tests that calling stopCapture while the permission prompt or engine startup
// is in-flight cancels startup cleanly.
TEST_F(TTCAudioSessionControllerTest,
       TestStartCaptureCancelledByStopCaptureDuringEngineStart) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionGranted);
  fake_engine_.deferStartCompletion = YES;

  base::test::TestFuture<BOOL, NSError*> future;
  [controller_
      startCaptureWithCompletion:base::CallbackToBlock(future.GetCallback())];

  ASSERT_NE(fake_engine_.pendingStartCompletion, nil);
  [controller_ stopCapture];

  fake_engine_.started = YES;
  fake_engine_.pendingStartCompletion(YES, nil);

  auto [success, error] = future.Take();
  EXPECT_FALSE(success);
  ASSERT_NE(error, nil);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(
                TTCAudioSessionControllerErrorCode::kStartupCancelled));
  EXPECT_FALSE(controller_.isCapturing);
  EXPECT_EQ(fake_session_manager_.restoreCallCount, 1u);
}

// Tests that if `startCapture` on the engine fails after the engine starts, the
// controller stops the engine and restores the audio session category.
TEST_F(TTCAudioSessionControllerTest,
       TestStartCaptureFailureRestoresSessionAndStopsEngine) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionGranted);
  fake_engine_.shouldFailStartCapture = YES;

  base::test::TestFuture<BOOL, NSError*> future;
  [controller_
      startCaptureWithCompletion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_FALSE(success);
  ASSERT_NE(error, nil);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(
                TTCAudioSessionControllerErrorCode::kCaptureStartFailed));
  EXPECT_FALSE(controller_.isCapturing);
  EXPECT_EQ(fake_engine_.stopCallCount, 1u);
  EXPECT_EQ(fake_session_manager_.restoreCallCount, 1u);
}

// Tests that playStreamingAudioChunk starts the engine on demand and queues
// chunks while the engine is starting, reporting `isPlaying == YES`
// immediately.
TEST_F(TTCAudioSessionControllerTest,
       TestPlayStreamingAudioChunkStartsEngineOnDemandAndQueuesChunks) {
  fake_engine_.deferStartCompletion = YES;

  int16_t sample1 = 1000;
  int16_t sample2 = 2000;
  NSData* chunk1 = [NSData dataWithBytes:&sample1 length:sizeof(sample1)];
  NSData* chunk2 = [NSData dataWithBytes:&sample2 length:sizeof(sample2)];

  [controller_ playStreamingAudioChunk:chunk1];
  [controller_ playStreamingAudioChunk:chunk2];

  // `isPlaying` is YES while chunks are queued waiting for the engine to start.
  EXPECT_TRUE(controller_.isPlaying);
  EXPECT_EQ(fake_engine_.startCallCount, 1u);
  EXPECT_EQ(fake_engine_.scheduledChunks.count, 0u);

  fake_engine_.started = YES;
  fake_engine_.pendingStartCompletion(YES, nil);

  EXPECT_EQ(fake_engine_.scheduledChunks.count, 2u);
  EXPECT_TRUE(controller_.isPlaying);
}

// Tests that loopbackEnabled defaults to NO and captured chunks are not routed
// to playback when disabled.
TEST_F(TTCAudioSessionControllerTest, TestLoopbackRoutingDisabled) {
  EXPECT_FALSE(controller_.isLoopbackEnabled);
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;

  int16_t samples[160] = {1000};
  NSData* chunk16k = [NSData dataWithBytes:samples length:sizeof(samples)];
  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk16k
                inputLevel:0.5f];

  EXPECT_EQ(fake_engine_.scheduledChunks.count, 0u);
  EXPECT_FALSE(controller_.isPlaying);
}

// Tests that setting loopbackEnabled to YES upsamples 16kHz captured chunks to
// 24kHz (3:2 sample ratio) and schedules them for playback.
TEST_F(TTCAudioSessionControllerTest, TestLoopbackRoutingEnabled) {
  controller_.loopbackEnabled = YES;
  EXPECT_TRUE(controller_.isLoopbackEnabled);
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;

  int16_t samples[160] = {1000};
  NSData* chunk16k = [NSData dataWithBytes:samples length:sizeof(samples)];
  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk16k
                inputLevel:0.5f];

  ASSERT_EQ(fake_engine_.scheduledChunks.count, 1u);
  // 160 samples at 16kHz -> 240 samples at 24kHz.
  EXPECT_EQ(fake_engine_.scheduledChunks.firstObject.length,
            240 * sizeof(int16_t));
  EXPECT_TRUE(controller_.isPlaying);

  [controller_ clearPlaybackQueue];
  EXPECT_FALSE(controller_.isPlaying);
}

// Tests that playing test tone while loopback is enabled halts loopback buffer
// routing during the tone, and stopping the tone cleanly restores loopback.
TEST_F(TTCAudioSessionControllerTest,
       TestPlayTestToneWhileLoopbackEnabledDoesNotBlockLoopback) {
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;
  controller_.loopbackEnabled = YES;

  int16_t samples[160] = {1000};
  NSData* chunk16k = [NSData dataWithBytes:samples length:sizeof(samples)];

  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk16k
                inputLevel:0.5f];
  EXPECT_TRUE(controller_.isPlaying);

  [controller_ playTestTone];
  EXPECT_TRUE(controller_.isPlaying);
  NSUInteger countAfterTone = fake_engine_.scheduledChunks.count;

  // While streaming playback (test tone) is active, loopback chunks are
  // suppressed.
  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk16k
                inputLevel:0.5f];
  EXPECT_EQ(fake_engine_.scheduledChunks.count, countAfterTone);

  [controller_ stopTestTone];
  EXPECT_FALSE(controller_.isPlaying);

  // Subsequent mic buffer routes to playback again.
  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk16k
                inputLevel:0.5f];
  EXPECT_TRUE(controller_.isPlaying);
}

// Tests that audioEngineDidStopPlayback does not stop the audio engine while
// a capture session is active.
TEST_F(TTCAudioSessionControllerTest,
       TestAudioEngineDidStopPlaybackDoesNotStopEngineWhenRecording) {
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;

  [controller_ audioEngineDidStopPlayback:fake_engine_];

  EXPECT_TRUE(fake_engine_.isStarted);
  EXPECT_TRUE(controller_.isCapturing);
  EXPECT_TRUE(delegate_.didStopPlayback);
}

// Tests that clearPlaybackQueue and stopPlayback stop active playback.
TEST_F(TTCAudioSessionControllerTest, TestAudioControllerPlaybackAndBargeIn) {
  fake_engine_.started = YES;

  [controller_ playTestTone];
  EXPECT_TRUE(controller_.isPlaying);

  [controller_ clearPlaybackQueue];
  EXPECT_FALSE(controller_.isPlaying);

  [controller_ playTestTone];
  EXPECT_TRUE(controller_.isPlaying);

  [controller_ stopPlayback];
  EXPECT_FALSE(controller_.isPlaying);
}

// Tests that audioEngine:didCaptureAudioData:inputLevel: forwards both input
// energy and PCM chunk to TTCAudioControllerDelegate.
TEST_F(TTCAudioSessionControllerTest,
       TestAudioControllerDelegatesCaptureChunkAndEnergy) {
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;

  int16_t sample = 12345;
  NSData* chunk = [NSData dataWithBytes:&sample length:sizeof(sample)];
  [controller_ audioEngine:fake_engine_
       didCaptureAudioData:chunk
                inputLevel:0.75f];

  EXPECT_FLOAT_EQ(delegate_.lastEnergy, 0.75f);
  EXPECT_NSEQ(delegate_.lastCapturedChunk, chunk);
}

// Tests that audioSessionManager:didChangeRouteDescription:hasHardwareAEC:
// forwards the route change event to TTCAudioControllerDelegate.
TEST_F(TTCAudioSessionControllerTest, TestRouteChangeDelegation) {
  EXPECT_FALSE(delegate_.didChangeRoute);
  [controller_ audioSessionManager:fake_session_manager_
         didChangeRouteDescription:@"In: TestMic | Out: TestSpeaker"
                    hasHardwareAEC:YES];
  EXPECT_TRUE(delegate_.didChangeRoute);

  controller_.delegate = nil;
  [controller_ audioSessionManager:fake_session_manager_
         didChangeRouteDescription:@"In: TestMic | Out: TestSpeaker"
                    hasHardwareAEC:NO];
}

// Tests that disconnect cleans up delegate references and disconnects both the
// engine and session manager.
TEST_F(TTCAudioSessionControllerTest, TestDisconnectClearsDelegates) {
  EXPECT_NSEQ(controller_.delegate, delegate_);
  EXPECT_NSEQ(fake_engine_.delegate, controller_);
  EXPECT_NSEQ(fake_session_manager_.delegate, controller_);

  [controller_ disconnect];

  EXPECT_NSEQ(controller_.delegate, nil);
  EXPECT_NSEQ(fake_engine_.delegate, nil);
  EXPECT_NSEQ(fake_session_manager_.delegate, nil);
  EXPECT_TRUE(fake_engine_.didDisconnect);
  EXPECT_TRUE(fake_session_manager_.didDisconnect);
}

// Tests that isOutputRoutedToSpeaker forwards state from session manager.
TEST_F(TTCAudioSessionControllerTest, TestOutputRoutedToSpeaker) {
  fake_session_manager_.mockOutputRoutedToSpeaker = YES;
  EXPECT_TRUE(controller_.isOutputRoutedToSpeaker);

  fake_session_manager_.mockOutputRoutedToSpeaker = NO;
  EXPECT_FALSE(controller_.isOutputRoutedToSpeaker);
}

// Tests that starting capture configures `aecMode` on the audio engine as
// `kHardware` when the active input route supports hardware AEC, and
// `kAdaptiveSoftware` otherwise.
TEST_F(TTCAudioSessionControllerTest,
       TestAECModeHardwareVsAdaptiveSoftwareOnStart) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionGranted);
  fake_session_manager_.mockHasHardwareAEC = YES;

  base::test::TestFuture<BOOL, NSError*> hw_future;
  [controller_ startCaptureWithCompletion:base::CallbackToBlock(
                                              hw_future.GetCallback())];
  auto [hw_success, hw_error] = hw_future.Take();
  EXPECT_TRUE(hw_success);
  EXPECT_EQ(fake_engine_.aecMode, TTCAudioAECMode::kHardware);

  [controller_ stopCapture];
  fake_session_manager_.mockHasHardwareAEC = NO;

  base::test::TestFuture<BOOL, NSError*> sw_future;
  [controller_ startCaptureWithCompletion:base::CallbackToBlock(
                                              sw_future.GetCallback())];
  auto [sw_success, sw_error] = sw_future.Take();
  EXPECT_TRUE(sw_success);
  EXPECT_EQ(fake_engine_.aecMode, TTCAudioAECMode::kAdaptiveSoftware);
}

// Tests that route change callbacks update `aecMode` on the audio engine.
TEST_F(TTCAudioSessionControllerTest, TestRouteChangeUpdatesAECMode) {
  [controller_ audioSessionManager:fake_session_manager_
         didChangeRouteDescription:@"In: Built-In Mic | Out: Speaker"
                    hasHardwareAEC:YES];
  EXPECT_EQ(fake_engine_.aecMode, TTCAudioAECMode::kHardware);

  [controller_ audioSessionManager:fake_session_manager_
         didChangeRouteDescription:@"In: Bluetooth HFP | Out: Bluetooth HFP"
                    hasHardwareAEC:NO];
  EXPECT_EQ(fake_engine_.aecMode, TTCAudioAECMode::kAdaptiveSoftware);
}

// Tests that `audioSessionManagerDidRequireEngineReconfiguration:` stops and
// restarts an active audio engine, restoring capture and updating `aecMode`.
TEST_F(TTCAudioSessionControllerTest,
       TestEngineReconfigurationRestartsEngineAndCapture) {
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;
  fake_session_manager_.mockHasHardwareAEC = NO;

  [controller_
      audioSessionManagerDidRequireEngineReconfiguration:fake_session_manager_];

  EXPECT_EQ(fake_engine_.stopCallCount, 1u);
  EXPECT_EQ(fake_engine_.startCallCount, 1u);
  EXPECT_EQ(fake_engine_.startCaptureCallCount, 1u);
  EXPECT_TRUE(fake_engine_.isStarted);
  EXPECT_TRUE(controller_.isCapturing);
  EXPECT_EQ(fake_engine_.aecMode, TTCAudioAECMode::kAdaptiveSoftware);
}

// Tests that `audioSessionManagerDidRequireEngineReconfiguration:` is a no-op
// when the audio engine is stopped and not starting.
TEST_F(TTCAudioSessionControllerTest,
       TestEngineReconfigurationIgnoredWhenEngineStopped) {
  EXPECT_FALSE(fake_engine_.isStarted);

  [controller_
      audioSessionManagerDidRequireEngineReconfiguration:fake_session_manager_];

  EXPECT_EQ(fake_engine_.stopCallCount, 0u);
  EXPECT_EQ(fake_engine_.startCallCount, 0u);
}

// Tests that `audioSessionManagerDidBeginInterruption:` stops active playback
// and capture and notifies the delegate.
TEST_F(TTCAudioSessionControllerTest,
       TestInterruptionBeganStopsCaptureAndPlayback) {
  fake_engine_.started = YES;
  fake_engine_.capturing = YES;
  [controller_ playTestTone];
  EXPECT_TRUE(controller_.isCapturing);
  EXPECT_TRUE(controller_.isPlaying);

  [controller_ audioSessionManagerDidBeginInterruption:fake_session_manager_];

  EXPECT_FALSE(controller_.isCapturing);
  EXPECT_FALSE(controller_.isPlaying);
  EXPECT_FALSE(fake_engine_.isStarted);
  EXPECT_TRUE(delegate_.didStopCapture);
}

}  // namespace
