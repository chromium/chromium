// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"

#import <AVFAudio/AVFAudio.h>

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/compiler_specific.h"
#import "base/containers/span.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder_delegate.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_audio_engine_protocol.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Expose internal delegate conformance and testing helpers on TTCAudioEngine.
@interface TTCAudioEngine (Testing) <TTCAudioRecorderDelegate,
                                     TTCAudioPlayerDelegate>
- (void)setIsCapturingForTesting:(BOOL)isCapturing;
- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning;
- (AVAudioEngine*)audioEngineForTesting;
@end

// Test player subclass that records `resumePlaybackAfterEngineRestart` calls.
@interface FakeReconfigTTCAudioPlayer : TTCAudioPlayer
@property(nonatomic, assign) BOOL didResumePlaybackAfterRestart;
@property(nonatomic, copy) void (^onResumePlayback)(void);
@end

@implementation FakeReconfigTTCAudioPlayer

- (void)resumePlaybackAfterEngineRestart {
  [super resumePlaybackAfterEngineRestart];
  self.didResumePlaybackAfterRestart = YES;
  if (self.onResumePlayback) {
    auto block = self.onResumePlayback;
    self.onResumePlayback = nil;
    block();
  }
}

@end

// Fake delegate to verify TTCAudioEngine forwards TTCAudioEngineDelegate
// events.
@interface FakeTTCAudioEngineDelegate : NSObject <TTCAudioEngineDelegate>
@property(nonatomic, strong) NSData* lastCapturedData;
@property(nonatomic, assign) float lastInputLevel;
@property(nonatomic, assign) BOOL didStartPlayback;
@property(nonatomic, assign) BOOL didStopPlayback;
@property(nonatomic, strong) NSError* lastError;
@end

@implementation FakeTTCAudioEngineDelegate

- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didCaptureAudioData:(NSData*)audioData
             inputLevel:(float)inputLevel {
  self.lastCapturedData = audioData;
  self.lastInputLevel = inputLevel;
}

- (void)audioEngineDidStartPlayback:(id<TTCAudioEngineProtocol>)engine {
  self.didStartPlayback = YES;
}

- (void)audioEngineDidStopPlayback:(id<TTCAudioEngineProtocol>)engine {
  self.didStopPlayback = YES;
}

- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didEncounterError:(NSError*)error {
  self.lastError = error;
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
  ASSERT_NE(engine, nil);

  EXPECT_FALSE(engine.isStarted);
  EXPECT_FALSE(engine.isCapturing);
  EXPECT_FALSE(engine.isPlaying);
  EXPECT_FLOAT_EQ(engine.inputAudioLevel, 0.0f);
  EXPECT_EQ(engine.aecMode, TTCAudioAECMode::kUnknown);

  [engine stopCapture];
  [engine stopPlayback];
  [engine disconnect];

  EXPECT_FALSE(engine.isCapturing);
}

// Tests that multiple stop and disconnect cycles can be invoked cleanly.
TEST_F(TTCAudioEngineTest, TestAudioEngineStopCycles) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_NE(engine, nil);

  for (int i = 0; i < 5; ++i) {
    [engine stopCapture];
    [engine stopPlayback];
  }
  [engine disconnect];

  EXPECT_FALSE(engine.isCapturing);
}

// Tests that calling disconnect cleanly stops capturing and cleans up state.
TEST_F(TTCAudioEngineTest, TestAudioEngineDisconnect) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  ASSERT_NE(engine, nil);
  [engine setIsAudioEngineRunningForTesting:YES];
  EXPECT_TRUE([engine startCapture]);
  EXPECT_TRUE(engine.isCapturing);

  [engine disconnect];

  EXPECT_FALSE(engine.isStarted);
  EXPECT_FALSE(engine.isCapturing);
  EXPECT_FALSE(engine.isPlaying);
}

// Tests that startWithCompletion and stopWithCompletion transition `started`
// state.
TEST_F(TTCAudioEngineTest, TestAudioEngineStartAndStopGraph) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  [engine setIsAudioEngineRunningForTesting:YES];
  [engine stopWithCompletion:nil];
  EXPECT_FALSE(engine.isStarted);

  [engine setIsAudioEngineRunningForTesting:YES];
  base::test::TestFuture<BOOL, NSError*> start_future;
  [engine
      startWithCompletion:base::CallbackToBlock(start_future.GetCallback())];
  auto [started, start_error] = start_future.Take();
  EXPECT_TRUE(started);
  EXPECT_NSEQ(start_error, nil);
  EXPECT_TRUE(engine.isStarted);

  base::test::TestFuture<BOOL, NSError*> stop_future;
  [engine stopWithCompletion:base::CallbackToBlock(stop_future.GetCallback())];
  auto [stopped, stop_error] = stop_future.Take();
  EXPECT_TRUE(stopped);
  EXPECT_NSEQ(stop_error, nil);
  EXPECT_FALSE(engine.isStarted);
  [engine disconnect];
}

// Tests that startCapture returns NO when the engine is not started.
TEST_F(TTCAudioEngineTest, TestStartCaptureRequiresStartedEngine) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  EXPECT_FALSE(engine.isStarted);
  EXPECT_FALSE([engine startCapture]);
  EXPECT_FALSE(engine.isCapturing);
  [engine disconnect];
}

// Tests that startCapture and stopCapture toggle capturing state when started.
TEST_F(TTCAudioEngineTest, TestStartAndStopCapture) {
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] init];
  [engine setIsAudioEngineRunningForTesting:YES];
  EXPECT_TRUE(engine.isStarted);

  EXPECT_TRUE([engine startCapture]);
  EXPECT_TRUE(engine.isCapturing);

  [engine stopCapture];
  EXPECT_FALSE(engine.isCapturing);
  EXPECT_TRUE(engine.isStarted);
  [engine disconnect];
}

// Tests that TTCAudioEngine converts Float32 PCM buffers from TTCAudioRecorder
// to Int16 NSData and forwards both audio data and clamped input level to its
// delegate.
TEST_F(TTCAudioEngineTest, TestAudioEngineDelegatesCapturedDataAndInputLevel) {
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:recorder
                                                                player:nil];
  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
  engine.delegate = delegate;
  [engine setIsCapturingForTesting:YES];

  [engine audioRecorder:recorder didUpdateInputEnergy:0.75f];
  EXPECT_FLOAT_EQ(engine.inputAudioLevel, 0.75f);

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

  ASSERT_NE(delegate.lastCapturedData, nil);
  EXPECT_EQ(delegate.lastCapturedData.length, 160 * sizeof(int16_t));
  EXPECT_FLOAT_EQ(delegate.lastInputLevel, 0.75f);
  auto samples = base::subtle::reinterpret_span<const int16_t>(
      base::apple::NSDataToSpan(delegate.lastCapturedData));
  EXPECT_NEAR(samples[0], 16383, 10);
  [engine disconnect];
}

// Tests that TTCAudioEngine forwards TTCAudioPlayerDelegate events to
// TTCAudioEngineDelegate.
TEST_F(TTCAudioEngineTest, TestAudioEnginePlaybackDelegation) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:nil
                                                                player:player];
  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
  engine.delegate = delegate;

  [engine audioPlayerDidStartPlayback:player];
  EXPECT_TRUE(delegate.didStartPlayback);

  [engine audioPlayerDidStopPlayback:player];
  EXPECT_TRUE(delegate.didStopPlayback);

  NSError* testError = [NSError errorWithDomain:@"test" code:42 userInfo:nil];
  [engine audioPlayer:player didEncounterError:testError];
  EXPECT_NSEQ(delegate.lastError, testError);

  [engine disconnect];
}

// Tests that schedulePlaybackData and stopPlayback manage playback on the
// player when the engine is started.
TEST_F(TTCAudioEngineTest, TestScheduleAndStopPlayback) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:nil
                                                                player:player];
  [engine setIsAudioEngineRunningForTesting:YES];

  int16_t sample = 1000;
  NSData* chunk = [NSData dataWithBytes:&sample length:sizeof(sample)];
  [engine schedulePlaybackData:chunk];
  EXPECT_TRUE(engine.isPlaying);

  [engine notifyEndOfPlaybackData];
  [engine stopPlayback];
  EXPECT_FALSE(engine.isPlaying);
  [engine disconnect];
}

// Tests that disconnect cleans up delegate references to prevent dangling
// calls.
TEST_F(TTCAudioEngineTest, TestDisconnectClearsDelegates) {
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:recorder
                                                                player:player];
  FakeTTCAudioEngineDelegate* delegate =
      [[FakeTTCAudioEngineDelegate alloc] init];
  engine.delegate = delegate;
  EXPECT_NSEQ(engine.delegate, delegate);
  EXPECT_NSEQ(recorder.delegate, engine);
  EXPECT_NSEQ(player.delegate, engine);

  [engine disconnect];

  EXPECT_NSEQ(engine.delegate, nil);
  EXPECT_NSEQ(recorder.delegate, nil);
  EXPECT_NSEQ(player.delegate, nil);
}

// Tests that AVAudioEngineConfigurationChangeNotification for the engine's
// internal AVAudioEngine resumes active playback.
TEST_F(TTCAudioEngineTest,
       TestEngineConfigurationChangeNotificationResumesPlayback) {
  FakeReconfigTTCAudioPlayer* player =
      [[FakeReconfigTTCAudioPlayer alloc] init];
  [player setIsPlayingForTesting:YES];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:nil
                                                                player:player];
  [engine setIsAudioEngineRunningForTesting:YES];

  base::test::TestFuture<void> resume_future;
  player.onResumePlayback = base::CallbackToBlock(resume_future.GetCallback());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioEngineConfigurationChangeNotification
                    object:[engine audioEngineForTesting]];

  EXPECT_TRUE(resume_future.Wait());
  EXPECT_TRUE(player.didResumePlaybackAfterRestart);
  [engine disconnect];
}

// Tests that AVAudioEngineConfigurationChangeNotification is ignored after
// the engine has been disconnected.
TEST_F(TTCAudioEngineTest,
       TestEngineConfigurationChangeNotificationIgnoredWhenDisconnected) {
  FakeReconfigTTCAudioPlayer* player =
      [[FakeReconfigTTCAudioPlayer alloc] init];
  [player setIsPlayingForTesting:YES];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:nil
                                                                player:player];
  [engine setIsAudioEngineRunningForTesting:YES];
  AVAudioEngine* rawEngine = [engine audioEngineForTesting];
  [engine disconnect];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioEngineConfigurationChangeNotification
                    object:rawEngine];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(player.didResumePlaybackAfterRestart);
}

// Tests that AVAudioEngineConfigurationChangeNotification posted by an
// unrelated AVAudioEngine instance is ignored.
TEST_F(TTCAudioEngineTest,
       TestEngineConfigurationChangeNotificationForDifferentEngineIgnored) {
  FakeReconfigTTCAudioPlayer* player =
      [[FakeReconfigTTCAudioPlayer alloc] init];
  [player setIsPlayingForTesting:YES];
  TTCAudioEngine* engine = [[TTCAudioEngine alloc] initWithAudioEngine:nil
                                                              recorder:nil
                                                                player:player];
  [engine setIsAudioEngineRunningForTesting:YES];

  AVAudioEngine* unrelatedEngine = [[AVAudioEngine alloc] init];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioEngineConfigurationChangeNotification
                    object:unrelatedEngine];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(player.didResumePlaybackAfterRestart);
  [engine disconnect];
}

}  // namespace
