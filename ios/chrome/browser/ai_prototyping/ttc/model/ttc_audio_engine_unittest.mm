// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"

#import <AVFAudio/AVFAudio.h>

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/compiler_specific.h"
#import "base/containers/span.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expose TTCAudioRecorderDelegate, TTCAudioPlayerDelegate, and testing helpers.
@interface TTCAudioEngine (Testing) <TTCAudioRecorderDelegate,
                                     TTCAudioPlayerDelegate>
- (void)setIsCapturingForTesting:(BOOL)isCapturing;
- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning;
@end

// Fake delegate to verify TTCAudioEngine forwards events.
@interface FakeTTCAudioControllerDelegate
    : NSObject <TTCAudioControllerDelegate>
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, assign) BOOL didStop;
@property(nonatomic, assign) BOOL didStartPlayback;
@property(nonatomic, assign) BOOL didStopPlayback;
@property(nonatomic, strong) NSError* lastError;
@property(nonatomic, strong) NSData* lastCapturedChunk;
@property(nonatomic, assign) BOOL didStartPlaybackViaController;
@property(nonatomic, assign) BOOL didStopPlaybackViaController;
@end

@implementation FakeTTCAudioControllerDelegate

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  _lastEnergy = energy;
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  _didStop = YES;
}

- (void)audioControllerDidStartPlayback:(id<TTCAudioController>)controller {
  _didStartPlayback = YES;
  _didStartPlaybackViaController = YES;
}

- (void)audioControllerDidStopPlayback:(id<TTCAudioController>)controller {
  _didStopPlayback = YES;
  _didStopPlaybackViaController = YES;
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  _lastError = error;
}

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  _lastCapturedChunk = pcmData;
}

@end

namespace {

class TTCAudioEngineTest : public PlatformTest {
 protected:
  void TearDown() override {
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
  id mock_audio_app_ = nil;
};

// Tests that TTCAudioEngine initializes with expected default properties.
TEST_F(TTCAudioEngineTest, TestAudioEngineDefaults) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  EXPECT_FALSE(engine.isCapturing);

  // Verifies that idempotent stops before starting do not crash or alter state.
  [engine stopCapture];
  [engine disconnect];

  EXPECT_FALSE(engine.isCapturing);
}

// Tests that multiple stop and disconnect cycles can be invoked cleanly.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopCycles) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  for (int i = 0; i < 5; ++i) {
    [engine stopCapture];
    [engine clearPlaybackQueue];
  }
  [engine disconnect];

  EXPECT_FALSE(engine.isCapturing);
}

// Tests that calling disconnect cleanly stops capturing and cleans up state.
TEST_F(TTCAudioEngineTest, TestAudioEngineDisconnect) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  [engine disconnect];

  EXPECT_FALSE(engine.isCapturing);
  EXPECT_FALSE(engine.isPlaying);
}

// Tests that TTCAudioEngine receives energy from TTCAudioRecorder and forwards
// it to its own delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineDelegatesEnergy) {
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:nil];
  ASSERT_TRUE(engine != nil);

  FakeTTCAudioControllerDelegate* delegate =
      [[FakeTTCAudioControllerDelegate alloc] init];
  engine.delegate = delegate;
  [engine setIsCapturingForTesting:YES];

  // Simulate recorder delegate callback on engine.
  [engine audioRecorder:recorder didUpdateInputEnergy:0.75f];

  EXPECT_FLOAT_EQ(delegate.lastEnergy, 0.75f);
  [engine disconnect];
}

// Tests that invoking stopCapture while audio session configuration is
// in flight cleanly cancels the startup sequence.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopWhileStartingCancelsRecording) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionGranted);

  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [engine startCaptureWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];

  // Stop capture while the background ThreadPool task is pending.
  [engine stopCapture];

  auto [success, error] = future.Get();
  EXPECT_FALSE(success);
  ASSERT_TRUE(error != nil);
  EXPECT_NSEQ(error.domain, kTTCAudioEngineErrorDomain);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(TTCAudioEngineErrorCode::kStartupCancelled));
  EXPECT_FALSE(engine.isCapturing);
}

// Tests that startCaptureWithCompletion fails with permission denied when
// microphone permission is not granted.
TEST_F(TTCAudioEngineTest, TestAudioEngineStartCapturePermissionDenied) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionDenied);

  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [engine startCaptureWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];

  auto [success, error] = future.Get();
  EXPECT_FALSE(success);
  ASSERT_TRUE(error != nil);
  EXPECT_NSEQ(error.domain, kTTCAudioEngineErrorDomain);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(TTCAudioEngineErrorCode::kPermissionDenied));
  EXPECT_FALSE(engine.isCapturing);
}

// Tests that startCaptureWithCompletion requests permission when undetermined
// and cancels when permission is denied by the user.
TEST_F(TTCAudioEngineTest,
       TestAudioEngineStartCapturePermissionRequestedAndDenied) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionUndetermined);
  OCMStub(ClassMethod([mock_audio_app_
              requestRecordPermissionWithCompletionHandler:[OCMArg any]]))
      .andDo(^(NSInvocation* invocation) {
        void (^handler)(BOOL);
        [invocation getArgument:&handler atIndex:2];
        handler(NO);
      });

  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [engine startCaptureWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];

  auto [success, error] = future.Get();
  EXPECT_FALSE(success);
  ASSERT_TRUE(error != nil);
  EXPECT_NSEQ(error.domain, kTTCAudioEngineErrorDomain);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(TTCAudioEngineErrorCode::kPermissionDenied));
  EXPECT_FALSE(engine.isCapturing);
}

// Tests that invoking stopCapture while permission request is in flight cleanly
// cancels the startup sequence.
TEST_F(TTCAudioEngineTest,
       TestAudioEngineStopWhileRequestingPermissionCancelsRecording) {
  SetUpMockAudioApp(AVAudioApplicationRecordPermissionUndetermined);
  __block void (^savedHandler)(BOOL) = nil;
  OCMStub(ClassMethod([mock_audio_app_
              requestRecordPermissionWithCompletionHandler:[OCMArg any]]))
      .andDo(^(NSInvocation* invocation) {
        void (^handler)(BOOL);
        [invocation getArgument:&handler atIndex:2];
        savedHandler = [handler copy];
      });

  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [engine startCaptureWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];

  // Stop capture while permission request is pending.
  [engine stopCapture];

  // Now invoke the permission handler.
  ASSERT_TRUE(savedHandler != nil);
  savedHandler(YES);

  auto [success, error] = future.Get();
  EXPECT_FALSE(success);
  ASSERT_TRUE(error != nil);
  EXPECT_NSEQ(error.domain, kTTCAudioEngineErrorDomain);
  EXPECT_EQ(error.code,
            static_cast<NSInteger>(TTCAudioEngineErrorCode::kStartupCancelled));
  EXPECT_FALSE(engine.isCapturing);
}

// Tests that invoking stopCapture while actively capturing notifies the
// delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopRecordingNotifiesDelegate) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  FakeTTCAudioControllerDelegate* delegate =
      [[FakeTTCAudioControllerDelegate alloc] init];
  engine.delegate = delegate;

  EXPECT_FALSE(delegate.didStop);

  [engine setIsCapturingForTesting:YES];
  EXPECT_TRUE(engine.isCapturing);

  [engine stopCapture];

  EXPECT_FALSE(engine.isCapturing);
  EXPECT_TRUE(delegate.didStop);

  // Verify that an idempotent subsequent stop does not re-trigger the delegate.
  delegate.didStop = NO;
  [engine stopCapture];
  EXPECT_FALSE(delegate.didStop);
}

// Tests toggling loopbackEnabled property on TTCAudioEngine.
TEST_F(TTCAudioEngineTest, TestLoopbackEnabledToggle) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];

  EXPECT_FALSE(engine.loopbackEnabled);

  engine.loopbackEnabled = YES;
  EXPECT_TRUE(engine.loopbackEnabled);

  engine.loopbackEnabled = NO;
  EXPECT_FALSE(engine.loopbackEnabled);
}

// Tests that playback delegate events are properly forwarded to engine
// delegate.
TEST_F(TTCAudioEngineTest, TestPlaybackDelegateForwarding) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];

  FakeTTCAudioControllerDelegate* delegate =
      [[FakeTTCAudioControllerDelegate alloc] init];
  engine.delegate = delegate;

  EXPECT_FALSE(delegate.didStartPlayback);
  EXPECT_FALSE(delegate.didStopPlayback);

  // Simulate player delegate start and stop callbacks.
  [engine audioPlayerDidStartPlayback:player];
  EXPECT_TRUE(delegate.didStartPlayback);

  [engine audioPlayerDidStopPlayback:player];
  EXPECT_TRUE(delegate.didStopPlayback);
}

// Tests stopTestTone stops playback.
TEST_F(TTCAudioEngineTest, TestStopTestTone) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];

  // Stop test tone when idle is a clean no-op.
  [engine stopTestTone];
  EXPECT_FALSE(engine.isPlaying);
}

// Tests that player errors are forwarded to engine delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineDelegatesPlayerError) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];

  FakeTTCAudioControllerDelegate* delegate =
      [[FakeTTCAudioControllerDelegate alloc] init];
  engine.delegate = delegate;

  NSError* testError = [NSError errorWithDomain:@"test_domain"
                                           code:-42
                                       userInfo:nil];
  [engine audioPlayer:player didEncounterError:testError];

  EXPECT_NSEQ(delegate.lastError, testError);
}

// Tests that loopback mic buffers are discarded when loopbackEnabled is NO.
TEST_F(TTCAudioEngineTest, TestAudioEngineLoopbackRoutingDisabled) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:player];

  [engine setIsCapturingForTesting:YES];
  engine.loopbackEnabled = NO;

  AVAudioFormat* format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                           frameCapacity:160];
  buffer.frameLength = 160;

  [engine audioRecorder:recorder didCaptureBuffer:buffer];

  EXPECT_FALSE(player.isPlaying);
  EXPECT_FALSE(engine.isPlaying);
  [engine disconnect];
}

// Tests that loopback mic buffers are routed to audio player when
// loopbackEnabled is YES.
TEST_F(TTCAudioEngineTest, TestAudioEngineLoopbackRoutingEnabled) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:player];

  [engine setIsCapturingForTesting:YES];
  engine.loopbackEnabled = YES;

  AVAudioFormat* format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                           frameCapacity:160];
  buffer.frameLength = 160;

  [engine audioRecorder:recorder didCaptureBuffer:buffer];

  EXPECT_TRUE(player.isPlaying);
  EXPECT_TRUE(engine.isPlaying);

  [engine clearPlaybackQueue];
  EXPECT_FALSE(player.isPlaying);
  EXPECT_FALSE(engine.isPlaying);
  [engine disconnect];
}


// Tests that playing test tone while loopback is enabled halts loopback buffer
// routing during the tone, and stopping the tone cleanly restores loopback.
TEST_F(TTCAudioEngineTest,
       TestPlayTestToneWhileLoopbackEnabledDoesNotBlockLoopback) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:player];
  [engine setIsAudioEngineRunningForTesting:YES];
  [engine setIsCapturingForTesting:YES];

  engine.loopbackEnabled = YES;

  AVAudioFormat* format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                           frameCapacity:160];
  buffer.frameLength = 160;

  // Initial loopback buffer routes to player.
  [engine audioRecorder:recorder didCaptureBuffer:buffer];
  EXPECT_TRUE(player.isPlaying);
  EXPECT_TRUE(engine.isPlaying);

  // Play test tone.
  [engine playTestTone];
  EXPECT_TRUE(player.isPlaying);
  EXPECT_TRUE(engine.isPlaying);

  // Stop test tone halts playback cleanly.
  [engine stopTestTone];
  EXPECT_FALSE(player.isPlaying);
  EXPECT_FALSE(engine.isPlaying);

  // Subsequent mic buffer routes to player without deadlock or starvation.
  [engine audioRecorder:recorder didCaptureBuffer:buffer];
  EXPECT_TRUE(player.isPlaying);
  EXPECT_TRUE(engine.isPlaying);

  [engine clearPlaybackQueue];
  [engine disconnect];
}

// Tests that audioPlayerDidStopPlayback does not stop the audio engine while
// a capture session is active or starting.
TEST_F(TTCAudioEngineTest,
       TestAudioPlayerDidStopPlaybackDoesNotStopEngineWhenRecording) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];

  [engine setIsCapturingForTesting:YES];
  EXPECT_TRUE(engine.isCapturing);

  // Simulate player reporting playback stopped while capture is active.
  [engine audioPlayerDidStopPlayback:player];

  // Verifies that capture remains active and did not trigger an engine stop.
  EXPECT_TRUE(engine.isCapturing);
}

// Tests that startCapture and stopCapture toggle capturing state via
// TTCAudioController.
TEST_F(TTCAudioEngineTest, TestAudioControllerCaptureMethods) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  id<TTCAudioController> controller = engine;

  [engine setIsCapturingForTesting:YES];
  EXPECT_TRUE(controller.isCapturing);

  [controller stopCapture];
  EXPECT_FALSE(controller.isCapturing);
  [controller disconnect];
}

// Tests that clearPlaybackQueue and stopPlayback stop active playback via
// TTCAudioController.
TEST_F(TTCAudioEngineTest, TestAudioControllerPlaybackAndBargeIn) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];
  id<TTCAudioController> controller = engine;
  [engine setIsAudioEngineRunningForTesting:YES];

  [controller playTestTone];
  EXPECT_TRUE(controller.isPlaying);

  [controller clearPlaybackQueue];
  EXPECT_FALSE(controller.isPlaying);

  [controller playTestTone];
  EXPECT_TRUE(controller.isPlaying);

  [controller stopPlayback];
  EXPECT_FALSE(controller.isPlaying);
  [controller disconnect];
}

// Tests that audioRecorder:didCaptureBuffer: converts PCM buffer to
// NSData and delivers it to audioController:didCaptureAudioChunk:.
TEST_F(TTCAudioEngineTest, TestAudioControllerDelegatesCaptureChunk) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:player];
  FakeTTCAudioControllerDelegate* delegate =
      [[FakeTTCAudioControllerDelegate alloc] init];
  engine.delegate = delegate;
  [engine setIsCapturingForTesting:YES];

  AVAudioFormat* format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                           frameCapacity:160];
  buffer.frameLength = 160;
  // SAFETY: `buffer.floatChannelData[0]` has capacity 160 guaranteed by
  // `initWithPCMFormat:frameCapacity:160` above.
  auto channelData =
      UNSAFE_BUFFERS(base::span(buffer.floatChannelData[0], 160u));
  std::fill(channelData.begin(), channelData.end(), 0.5f);

  [engine audioRecorder:recorder didCaptureBuffer:buffer];

  ASSERT_NE(delegate.lastCapturedChunk, nil);
  EXPECT_EQ(delegate.lastCapturedChunk.length, 160 * sizeof(int16_t));
  auto samples = base::subtle::reinterpret_span<const int16_t>(
      base::apple::NSDataToSpan(delegate.lastCapturedChunk));
  EXPECT_NEAR(samples[0], 16383, 10);
  [engine disconnect];
}

}  // namespace
