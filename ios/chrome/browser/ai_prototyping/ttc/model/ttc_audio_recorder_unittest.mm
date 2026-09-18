// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"

#import <AVFAudio/AVFAudio.h>

#import <cmath>

#import "base/compiler_specific.h"
#import "base/functional/callback_helpers.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_metrics.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Expose handleInputBuffer: for unit testing.
@interface TTCAudioRecorder (Testing)
- (void)handleInputBuffer:(AVAudioPCMBuffer*)buffer;
@end

// Fake delegate to verify TTCAudioRecorder delegate notifications.
@interface FakeTTCAudioRecorderDelegate : NSObject <TTCAudioRecorderDelegate>
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, strong) AVAudioPCMBuffer* lastBuffer;
@property(nonatomic, assign) NSInteger energyCallbackCount;
@property(nonatomic, assign) NSInteger bufferCallbackCount;
@property(nonatomic, copy) void (^onEnergyDelivered)(float energy);
@property(nonatomic, copy) void (^onBufferDelivered)(AVAudioPCMBuffer* buffer);
@end

@implementation FakeTTCAudioRecorderDelegate

- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)rms {
  _lastEnergy = rms;
  _energyCallbackCount++;
  if (_onEnergyDelivered) {
    _onEnergyDelivered(rms);
  }
}

- (void)audioRecorder:(TTCAudioRecorder*)recorder
     didCaptureBuffer:(AVAudioPCMBuffer*)buffer {
  _lastBuffer = buffer;
  _bufferCallbackCount++;
  if (_onBufferDelivered) {
    _onBufferDelivered(buffer);
  }
}

@end

namespace {

using TTCAudioRecorderTest = PlatformTest;

// Tests that TTCAudioRecorder initializes with expected default properties.
TEST_F(TTCAudioRecorderTest, TestAudioRecorderDefaults) {
  web::WebTaskEnvironment task_environment;
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  ASSERT_TRUE(recorder != nil);

  EXPECT_FALSE(recorder.isRecording);
  EXPECT_FALSE(recorder.recording);

  // Verifies that idempotent stops before starting do not crash or alter state.
  [recorder removeTapFromInputNode:nil];
  [recorder reset];

  EXPECT_FALSE(recorder.isRecording);
  EXPECT_FALSE(recorder.recording);
}

// Tests that multiple stop and reset cycles can be invoked cleanly.
TEST_F(TTCAudioRecorderTest, TestAudioRecorderStopCycles) {
  web::WebTaskEnvironment task_environment;
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  ASSERT_TRUE(recorder != nil);

  for (int i = 0; i < 5; ++i) {
    [recorder removeTapFromInputNode:nil];
    [recorder reset];
  }

  EXPECT_FALSE(recorder.isRecording);
  EXPECT_FALSE(recorder.recording);
}

// Tests that AVAudioConverter correctly resamples audio buffers between sample
// rates (48kHz stereo to 16kHz mono).
TEST_F(TTCAudioRecorderTest, TestAudioConverterResampling) {
  AVAudioFormat* format48k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000.0
                                                     channels:2];
  AVAudioFormat* format16k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];

  AVAudioPCMBuffer* inBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format48k frameCapacity:4800];
  inBuffer.frameLength = 4800;  // 100ms of audio

  // Fill buffer with synthetic sine wave.
  // SAFETY: `inBuffer.floatChannelData` contains pointers for 2 channels,
  // each allocated with frame capacity >= `inBuffer.frameLength`.
  UNSAFE_BUFFERS({
    for (AVAudioFrameCount i = 0; i < inBuffer.frameLength; ++i) {
      float sample = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
      inBuffer.floatChannelData[0][i] = sample;
      inBuffer.floatChannelData[1][i] = sample;
    }
  });

  AVAudioConverter* converter =
      [[AVAudioConverter alloc] initFromFormat:format48k toFormat:format16k];
  ASSERT_TRUE(converter != nil);

  AVAudioPCMBuffer* outBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format16k frameCapacity:1600];

  __block BOOL inputConsumed = NO;
  NSError* conversionError = nil;
  AVAudioConverterOutputStatus status =
      [converter convertToBuffer:outBuffer
                           error:&conversionError
              withInputFromBlock:^AVAudioBuffer*(
                  AVAudioPacketCount inNumberOfPackets,
                  AVAudioConverterInputStatus* outStatus) {
                if (!inputConsumed) {
                  inputConsumed = YES;
                  *outStatus = AVAudioConverterInputStatus_HaveData;
                  return inBuffer;
                }
                *outStatus = AVAudioConverterInputStatus_NoDataNow;
                return nil;
              }];

  EXPECT_NE(status, AVAudioConverterOutputStatus_Error);
  EXPECT_TRUE(conversionError == nil);
  EXPECT_GT(outBuffer.frameLength, 0u);
  EXPECT_EQ(outBuffer.format.sampleRate, 16000.0);
  EXPECT_EQ(outBuffer.format.channelCount, 1u);
}

// Tests that feeding a 48kHz stereo buffer into handleInputBuffer correctly
// resamples the buffer, calculates energy, and delivers both to the delegate.
TEST_F(TTCAudioRecorderTest,
       TestHandleInputBufferWithResamplingDeliversBufferAndEnergy) {
  web::WebTaskEnvironment task_environment;
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  ASSERT_TRUE(recorder != nil);

  FakeTTCAudioRecorderDelegate* delegate =
      [[FakeTTCAudioRecorderDelegate alloc] init];
  recorder.delegate = delegate;

  base::test::TestFuture<float> energy_future;
  base::test::TestFuture<AVAudioPCMBuffer*> buffer_future;

  delegate.onEnergyDelivered =
      base::CallbackToBlock(energy_future.GetCallback());
  delegate.onBufferDelivered =
      base::CallbackToBlock(buffer_future.GetCallback());

  AVAudioFormat* format48k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000.0
                                                     channels:2];
  AVAudioPCMBuffer* inBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format48k frameCapacity:4800];
  inBuffer.frameLength = 4800;

  UNSAFE_BUFFERS({
    for (AVAudioFrameCount i = 0; i < inBuffer.frameLength; ++i) {
      float sample = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
      inBuffer.floatChannelData[0][i] = sample;
      inBuffer.floatChannelData[1][i] = sample;
    }
  });

  [recorder handleInputBuffer:inBuffer];

  float energy = energy_future.Take();
  AVAudioPCMBuffer* captured_buffer = buffer_future.Take();

  EXPECT_GT(delegate.energyCallbackCount, 0);
  EXPECT_GT(energy, 0.0f);
  EXPECT_GT(delegate.bufferCallbackCount, 0);
  ASSERT_TRUE(captured_buffer != nil);
  EXPECT_EQ(captured_buffer.format.sampleRate, 16000.0);
  EXPECT_EQ(captured_buffer.format.channelCount, 1u);
  EXPECT_GT(captured_buffer.frameLength, 0u);
}

// Tests that feeding a 16kHz mono Float32 buffer into handleInputBuffer
// forwards energy and buffer without resampling errors.
TEST_F(TTCAudioRecorderTest,
       TestHandleInputBufferWithNative16kMonoDeliversBufferAndEnergy) {
  web::WebTaskEnvironment task_environment;
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  ASSERT_TRUE(recorder != nil);

  FakeTTCAudioRecorderDelegate* delegate =
      [[FakeTTCAudioRecorderDelegate alloc] init];
  recorder.delegate = delegate;

  base::test::TestFuture<float> energy_future;
  base::test::TestFuture<AVAudioPCMBuffer*> buffer_future;

  delegate.onEnergyDelivered =
      base::CallbackToBlock(energy_future.GetCallback());
  delegate.onBufferDelivered =
      base::CallbackToBlock(buffer_future.GetCallback());

  AVAudioFormat* format16k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* inBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format16k frameCapacity:1600];
  inBuffer.frameLength = 1600;

  UNSAFE_BUFFERS({
    for (AVAudioFrameCount i = 0; i < inBuffer.frameLength; ++i) {
      inBuffer.floatChannelData[0][i] = 0.5f;
    }
  });

  [recorder handleInputBuffer:inBuffer];

  float energy = energy_future.Take();
  AVAudioPCMBuffer* captured_buffer = buffer_future.Take();

  EXPECT_GT(delegate.energyCallbackCount, 0);
  EXPECT_NEAR(energy, ttc::LinearRmsToPerceptualLevel(0.5f), 0.01f);
  EXPECT_GT(delegate.bufferCallbackCount, 0);
  ASSERT_TRUE(captured_buffer != nil);
  EXPECT_EQ(captured_buffer.format.sampleRate, 16000.0);
  EXPECT_EQ(captured_buffer.format.channelCount, 1u);
}

// Tests that handleInputBuffer safely ignores nil or empty buffers.
TEST_F(TTCAudioRecorderTest, TestHandleInputBufferNilOrEmptyDoesNotCrash) {
  web::WebTaskEnvironment task_environment;
  TTCAudioRecorder* recorder = [[TTCAudioRecorder alloc] init];
  ASSERT_TRUE(recorder != nil);

  FakeTTCAudioRecorderDelegate* delegate =
      [[FakeTTCAudioRecorderDelegate alloc] init];
  recorder.delegate = delegate;

  [recorder handleInputBuffer:nil];

  AVAudioFormat* format16k =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:16000.0
                                                     channels:1];
  AVAudioPCMBuffer* emptyBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format16k frameCapacity:1600];
  emptyBuffer.frameLength = 0;

  [recorder handleInputBuffer:emptyBuffer];

  base::test::TestFuture<void> flush_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_EQ(delegate.energyCallbackCount, 0);
  EXPECT_EQ(delegate.bufferCallbackCount, 0);
}

}  // namespace
