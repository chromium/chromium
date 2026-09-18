// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"

#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Expose TTCAudioRecorderDelegate, TTCAudioPlayerDelegate, and testing helpers.
@interface TTCAudioEngine (Testing) <TTCAudioRecorderDelegate,
                                     TTCAudioPlayerDelegate>
- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
                          player:(TTCAudioPlayer*)player;
- (void)setIsRecordingForTesting:(BOOL)isRecording;
- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning;
@end

// Fake delegate to verify TTCAudioEngine forwards events.
@interface FakeTTCAudioEngineDelegate : NSObject <TTCAudioEngineDelegate>
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, assign) BOOL didStop;
@property(nonatomic, assign) BOOL didStartPlayback;
@property(nonatomic, assign) BOOL didStopPlayback;
@property(nonatomic, strong) NSError* lastError;
@end

@implementation FakeTTCAudioEngineDelegate

- (void)audioEngine:(TTCAudioEngine*)engine didUpdateInputEnergy:(float)rms {
  _lastEnergy = rms;
}

- (void)audioEngineDidStopRecording:(TTCAudioEngine*)engine {
  _didStop = YES;
}

- (void)audioEngineDidStartPlayback:(TTCAudioEngine*)engine {
  _didStartPlayback = YES;
}

- (void)audioEngineDidStopPlayback:(TTCAudioEngine*)engine {
  _didStopPlayback = YES;
}

- (void)audioEngine:(TTCAudioEngine*)engine didEncounterError:(NSError*)error {
  _lastError = error;
}

@end

namespace {

class TTCAudioEngineTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that TTCAudioEngine initializes with expected default properties.
TEST_F(TTCAudioEngineTest, TestAudioEngineDefaults) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  EXPECT_FALSE(engine.isRecording);

  // Verifies that idempotent stops before starting do not crash or alter state.
  [engine stopRecording];
  [engine disconnect];

  EXPECT_FALSE(engine.isRecording);
}

// Tests that multiple stop and disconnect cycles can be invoked cleanly.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopCycles) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  for (int i = 0; i < 5; ++i) {
    [engine stopRecording];
    [engine stopPlaybackImmediately];
  }
  [engine disconnect];

  EXPECT_FALSE(engine.isRecording);
}

// Tests that calling disconnect cleanly stops recording and cleans up state.
TEST_F(TTCAudioEngineTest, TestAudioEngineDisconnect) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  [engine disconnect];

  EXPECT_FALSE(engine.isRecording);
  EXPECT_FALSE(engine.isPlaying);
}

// Tests that TTCAudioEngine receives energy from TTCAudioRecorder and forwards
// it to its own delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineDelegatesEnergy) {
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder];
  ASSERT_TRUE(engine != nil);

  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
  engine.delegate = delegate;

  // Simulate recorder delegate callback on engine.
  [engine audioRecorder:recorder didUpdateInputEnergy:0.75f];

  EXPECT_FLOAT_EQ(delegate.lastEnergy, 0.75f);
}

// Tests that invoking stopRecording while audio session configuration is
// in flight cleanly cancels the startup sequence.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopWhileStartingCancelsRecording) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [engine startRecordingWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];

  // Stop recording while the background ThreadPool task is pending.
  [engine stopRecording];

  auto [success, error] = future.Get();
  EXPECT_FALSE(success);
  EXPECT_TRUE(error != nil);
  EXPECT_FALSE(engine.isRecording);
}

// Tests that invoking stopRecording while actively recording notifies the
// delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopRecordingNotifiesDelegate) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_TRUE(engine != nil);

  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
  engine.delegate = delegate;

  EXPECT_FALSE(delegate.didStop);

  [engine setIsRecordingForTesting:YES];
  EXPECT_TRUE(engine.isRecording);

  [engine stopRecording];

  EXPECT_FALSE(engine.isRecording);
  EXPECT_TRUE(delegate.didStop);

  // Verify that an idempotent subsequent stop does not re-trigger the delegate.
  delegate.didStop = NO;
  [engine stopRecording];
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

  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
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

  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
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
}

// Tests that loopback mic buffers are routed to audio player when
// loopbackEnabled is YES.
TEST_F(TTCAudioEngineTest, TestAudioEngineLoopbackRoutingEnabled) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithRecorder:recorder
                                                             player:player];

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

  [engine stopPlaybackImmediately];
  EXPECT_FALSE(player.isPlaying);
  EXPECT_FALSE(engine.isPlaying);
}

// Tests that playTestTone synthesizes sine wave audio and begins playback, and
// stopTestTone halts it.
TEST_F(TTCAudioEngineTest, TestPlayAndStopTestTone) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];
  [engine setIsAudioEngineRunningForTesting:YES];

  [engine playTestTone];
  EXPECT_TRUE(player.isPlaying);
  EXPECT_TRUE(engine.isPlaying);

  [engine stopTestTone];
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

  [engine stopPlaybackImmediately];
  [engine disconnect];
}

// Tests that audioPlayerDidStopPlayback does not stop the audio engine while
// a recording session is active or starting.
TEST_F(TTCAudioEngineTest,
       TestAudioPlayerDidStopPlaybackDoesNotStopEngineWhenRecording) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine =
      [[TTCAudioEngine alloc] initWithRecorder:[[TTCAudioRecorder alloc] init]
                                        player:player];

  [engine setIsRecordingForTesting:YES];
  EXPECT_TRUE(engine.isRecording);

  // Simulate player reporting playback stopped while recording is active.
  [engine audioPlayerDidStopPlayback:player];

  // Verifies that recording remains active and did not trigger an engine stop.
  EXPECT_TRUE(engine.isRecording);
}

}  // namespace
