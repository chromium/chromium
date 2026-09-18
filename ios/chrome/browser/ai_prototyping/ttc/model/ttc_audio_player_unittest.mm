// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player.h"

#import <AVFAudio/AVFAudio.h>

#import <algorithm>
#import <vector>

#import "base/compiler_specific.h"
#import "base/containers/span.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake delegate implementation capturing TTCAudioPlayer lifecycle events.
@interface FakeTTCAudioPlayerDelegate : NSObject <TTCAudioPlayerDelegate>

@property(nonatomic, assign) NSInteger startPlaybackCount;
@property(nonatomic, assign) NSInteger stopPlaybackCount;
@property(nonatomic, strong) NSError* lastError;

@end

@implementation FakeTTCAudioPlayerDelegate

- (void)audioPlayerDidStartPlayback:(TTCAudioPlayer*)player {
  _startPlaybackCount++;
}

- (void)audioPlayerDidStopPlayback:(TTCAudioPlayer*)player {
  _stopPlaybackCount++;
}

- (void)audioPlayer:(TTCAudioPlayer*)player didEncounterError:(NSError*)error {
  _lastError = error;
}

@end

// Expose internal methods for testing.
@interface TTCAudioPlayer (Testing)
- (void)handleScheduledBufferCompletionForSession:(uint64_t)sessionId;
@end

class TTCAudioPlayerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    engine_ = [[AVAudioEngine alloc] init];
    player_ = [[TTCAudioPlayer alloc] init];
    NSError* error = nil;
    BOOL attached = [player_ attachToAudioEngine:engine_ error:&error];
    ASSERT_TRUE(attached);
    ASSERT_TRUE(error == nil);
    delegate_ = [[FakeTTCAudioPlayerDelegate alloc] init];
    player_.delegate = delegate_;
  }

  void TearDown() override {
    [player_ reset];
    if (engine_) {
      [player_ detachFromAudioEngine:engine_];
    }
    player_ = nil;
    engine_ = nil;
    delegate_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  AVAudioEngine* engine_ = nil;
  TTCAudioPlayer* player_ = nil;
  FakeTTCAudioPlayerDelegate* delegate_ = nil;
};

// Test ConvertInt16ToFloat32 properly normalizes signed 16-bit PCM samples to
// [-1.0, 1.0] floating point representation.
TEST_F(TTCAudioPlayerTest, TestConvertInt16ToFloat32Normalization) {
  const std::vector<int16_t> source = {0, 32767, -32768, 16384, -16384};
  std::vector<float> destination(source.size(), 0.0f);

  ttc::ConvertInt16ToFloat32(source, destination);

  EXPECT_FLOAT_EQ(destination[0], 0.0f);
  EXPECT_NEAR(destination[1], 1.0f, 0.001f);
  EXPECT_FLOAT_EQ(destination[2], -1.0f);
  EXPECT_FLOAT_EQ(destination[3], 0.5f);
  EXPECT_FLOAT_EQ(destination[4], -0.5f);
}

// Test ConvertInt16ToFloat32 safely handles empty source and destination spans.
TEST_F(TTCAudioPlayerTest, TestConvertInt16ToFloat32EmptySpans) {
  const std::vector<int16_t> source;
  std::vector<float> destination;

  // Should not crash or perform out-of-bounds operations.
  ttc::ConvertInt16ToFloat32(source, destination);
  EXPECT_TRUE(destination.empty());
}

// Test initial state of TTCAudioPlayer has isPlaying set to NO.
TEST_F(TTCAudioPlayerTest, TestInitialState) {
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 0);
  EXPECT_EQ(delegate_.stopPlaybackCount, 0);
}

// Test attaching player node to an AVAudioEngine graph and detaching safely.
TEST_F(TTCAudioPlayerTest, TestAttachAndDetachFromEngine) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  AVAudioEngine* engine = [[AVAudioEngine alloc] init];
  NSError* error = nil;

  BOOL attached = [player attachToAudioEngine:engine error:&error];
  EXPECT_TRUE(attached);
  EXPECT_EQ(error, nil);

  [player detachFromAudioEngine:engine];
}

// Test calling attachToAudioEngine multiple times on the same engine succeeds
// idempotently without throwing NSInvalidArgumentException.
TEST_F(TTCAudioPlayerTest, TestAttachToAudioEngineIdempotent) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  AVAudioEngine* engine = [[AVAudioEngine alloc] init];
  NSError* error = nil;

  EXPECT_TRUE([player attachToAudioEngine:engine error:&error]);
  EXPECT_EQ(error, nil);

  // Subsequent attachment on same engine should succeed and reconnect.
  EXPECT_TRUE([player attachToAudioEngine:engine error:&error]);
  EXPECT_EQ(error, nil);

  [player detachFromAudioEngine:engine];
}

// Test attachToAudioEngine returns NO when passed a nil engine.
TEST_F(TTCAudioPlayerTest, TestAttachNilEngineFailsGracefully) {
  TTCAudioPlayer* player = [[TTCAudioPlayer alloc] init];
  NSError* error = nil;
  BOOL attached = [player attachToAudioEngine:nil error:&error];
  EXPECT_FALSE(attached);
  EXPECT_NE(error, nil);
}

// Test playStreamingAudioChunk when unattached to an engine is a safe no-op.
TEST_F(TTCAudioPlayerTest, TestPlayWhenUnattachedIsSafeNoOp) {
  TTCAudioPlayer* unattachedPlayer = [[TTCAudioPlayer alloc] init];
  const std::vector<int16_t> samples(240, 100);
  NSData* pcmData = [NSData dataWithBytes:samples.data()
                                   length:samples.size() * sizeof(int16_t)];
  [unattachedPlayer playStreamingAudioChunk:pcmData];
  EXPECT_FALSE(unattachedPlayer.isPlaying);
}

// Test playStreamingAudioChunk with empty data is a no-op and does not begin
// playback.
TEST_F(TTCAudioPlayerTest, TestPlayEmptyChunkNoOp) {
  NSData* emptyData = [NSData data];
  [player_ playStreamingAudioChunk:emptyData];

  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 0);
}

// Test stopPlaybackImmediately stops player and invokes delegate stop callback
// when playback was active.
TEST_F(TTCAudioPlayerTest, TestStopPlaybackImmediatelyNotifiesDelegate) {
  [player_ setIsPlayingForTesting:YES];
  EXPECT_TRUE(player_.isPlaying);

  [player_ stopPlaybackImmediately];

  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 1);
}

// Test stopPlaybackImmediately when already idle is a no-op for delegate.
TEST_F(TTCAudioPlayerTest, TestStopPlaybackWhenIdleDoesNotNotifyDelegate) {
  EXPECT_FALSE(player_.isPlaying);

  [player_ stopPlaybackImmediately];

  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 0);
}

// Test playPCMBuffer with nil or empty buffer is a no-op.
TEST_F(TTCAudioPlayerTest, TestPlayPCMBufferNilOrEmpty) {
  [player_ playPCMBuffer:nil];
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 0);

  AVAudioFormat* format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:24000.0
                                                     channels:1];
  AVAudioPCMBuffer* emptyBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format frameCapacity:1024];
  emptyBuffer.frameLength = 0;

  [player_ playPCMBuffer:emptyBuffer];
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 0);
}

// Test reset purges playback state and stops player without spurious delegate
// callbacks.
TEST_F(TTCAudioPlayerTest, TestResetClearsState) {
  [player_ setIsPlayingForTesting:YES];
  EXPECT_TRUE(player_.isPlaying);

  [player_ reset];

  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 0);
}

// Test playStreamingAudioChunk schedules buffer, begins playback, and notifies
// delegate.
TEST_F(TTCAudioPlayerTest, TestPlayStreamingAudioChunkSchedulesBuffer) {
  const std::vector<int16_t> samples(480, 500);
  NSData* pcmData = [NSData dataWithBytes:samples.data()
                                   length:samples.size() * sizeof(int16_t)];

  [player_ playStreamingAudioChunk:pcmData];

  EXPECT_TRUE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 1);

  [player_ stopPlaybackImmediately];
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 1);
}

// Test that buffer completion callbacks with a stale session ID are ignored and
// do not prematurely stop active playback.
TEST_F(TTCAudioPlayerTest, TestStaleCompletionCallbackDiscarded) {
  const std::vector<int16_t> samples(240, 1000);
  NSData* pcmData = [NSData dataWithBytes:samples.data()
                                   length:samples.size() * sizeof(int16_t)];

  [player_ playStreamingAudioChunk:pcmData];
  EXPECT_TRUE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 1);

  // Invoke completion with a stale session ID.
  [player_ handleScheduledBufferCompletionForSession:999];

  // Playback should remain active because the stale completion was discarded.
  EXPECT_TRUE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 0);

  // Stop cleanly.
  [player_ stopPlaybackImmediately];
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 1);
}

// Test playPCMBuffer converts 16kHz buffer to 24kHz and schedules for playback.
TEST_F(TTCAudioPlayerTest, TestPlayPCMBufferWithResampling) {
  AVAudioFormat* format16k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* buffer16k =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format16k frameCapacity:160];
  buffer16k.frameLength = 160;
  // SAFETY: `buffer16k.floatChannelData[0]` has capacity of 160 floats
  // guaranteed by `initWithPCMFormat:frameCapacity:160` above.
  auto channelSpan = UNSAFE_BUFFERS(
      base::span(buffer16k.floatChannelData[0], static_cast<size_t>(160)));
  std::fill(channelSpan.begin(), channelSpan.end(), 0.5f);

  [player_ playPCMBuffer:buffer16k];

  EXPECT_TRUE(player_.isPlaying);
  EXPECT_EQ(delegate_.startPlaybackCount, 1);

  [player_ stopPlaybackImmediately];
  EXPECT_FALSE(player_.isPlaying);
  EXPECT_EQ(delegate_.stopPlaybackCount, 1);
}

// Test playPCMBuffer with zero sample rate is ignored safely without division
// by zero.
TEST_F(TTCAudioPlayerTest, TestPlayPCMBufferZeroSampleRateIgnored) {
  AudioStreamBasicDescription asbd = {};
  asbd.mSampleRate = 0.0;
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mChannelsPerFrame = 1;
  asbd.mBitsPerChannel = 32;
  asbd.mBytesPerFrame = 4;
  asbd.mBytesPerPacket = 4;
  asbd.mFramesPerPacket = 1;
  asbd.mFormatFlags = kAudioFormatFlagIsFloat;

  AVAudioFormat* zeroRateFormat =
      [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
  if (zeroRateFormat) {
    AVAudioPCMBuffer* buffer =
        [[AVAudioPCMBuffer alloc] initWithPCMFormat:zeroRateFormat
                                      frameCapacity:100];
    buffer.frameLength = 100;
    [player_ playPCMBuffer:buffer];
    EXPECT_FALSE(player_.isPlaying);
    EXPECT_EQ(delegate_.startPlaybackCount, 0);
  }
}
