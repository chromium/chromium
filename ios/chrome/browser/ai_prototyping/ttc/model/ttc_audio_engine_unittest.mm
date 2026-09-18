// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"

#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Expose TTCAudioRecorderDelegate conformance and testing helpers.
@interface TTCAudioEngine (Testing) <TTCAudioRecorderDelegate>
- (void)setIsRecordingForTesting:(BOOL)isRecording;
@end

// Fake delegate to verify TTCAudioEngine forwards events.
@interface FakeTTCAudioEngineDelegate : NSObject <TTCAudioEngineDelegate>
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, assign) BOOL didStop;
@end

@implementation FakeTTCAudioEngineDelegate

- (void)audioEngine:(TTCAudioEngine*)engine didUpdateInputEnergy:(float)rms {
  _lastEnergy = rms;
}

- (void)audioEngineDidStopRecording:(TTCAudioEngine*)engine {
  _didStop = YES;
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

}  // namespace
