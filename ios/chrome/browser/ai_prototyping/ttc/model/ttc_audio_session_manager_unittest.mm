// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/callback_helpers.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class TTCAudioSessionManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    manager_ = [[TTCAudioSessionManager alloc] init];
  }

  void TearDown() override {
    @autoreleasepool {
      [manager_ disconnect];
      manager_ = nil;
    }
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  TTCAudioSessionManager* manager_ = nil;
};

// Test that configureAudioSession configures the session synchronously on the
// UI thread.
TEST_F(TTCAudioSessionManagerTest, TestConfigureAudioSessionSynchronous) {
  NSError* error = [manager_ configureAudioSession];
  EXPECT_NSEQ(error, nil);

  AVAudioSession* session = [AVAudioSession sharedInstance];
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);
}

// Test that configureAudioSessionWithCompletion: configures the session
// asynchronously on the task runner and invokes completion on the UI thread.
TEST_F(TTCAudioSessionManagerTest, TestConfigureAudioSessionWithCompletion) {
  base::test::TestFuture<NSError*> future;
  [manager_ configureAudioSessionWithCompletion:base::CallbackToBlock(
                                                    future.GetCallback())];

  NSError* error = future.Take();
  EXPECT_NSEQ(error, nil);

  AVAudioSession* session = [AVAudioSession sharedInstance];
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);
}

// Test that restoreAudioSessionCategory restores the previous category, mode,
// and categoryOptions.
TEST_F(TTCAudioSessionManagerTest,
       TestRestoreAudioSessionCategoryModeAndOptions) {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* prime_error = nil;
  [session setCategory:AVAudioSessionCategorySoloAmbient
                  mode:AVAudioSessionModeDefault
               options:0
                 error:&prime_error];
  ASSERT_NSEQ(prime_error, nil);
  EXPECT_NSEQ(session.category, AVAudioSessionCategorySoloAmbient);

  NSError* config_error = [manager_ configureAudioSession];
  EXPECT_NSEQ(config_error, nil);
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);

  [manager_ restoreAudioSessionCategory];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return [session.category
                   isEqualToString:AVAudioSessionCategorySoloAmbient] &&
               session.categoryOptions == 0;
      }));
}

// Test that activeInputRouteName and activeOutputRouteName return valid names
// or nil when hardware ports are unattached.
TEST_F(TTCAudioSessionManagerTest, TestActiveRouteNames) {
  NSString* input_route = manager_.activeInputRouteName;
  if (input_route) {
    EXPECT_GT(input_route.length, 0u);
  }

  NSString* output_route = manager_.activeOutputRouteName;
  if (output_route) {
    EXPECT_GT(output_route.length, 0u);
  }
}

// Test that disconnect can be called safely multiple times without errors,
// and ensures the prior session state is restored.
TEST_F(TTCAudioSessionManagerTest, TestDisconnectIdempotence) {
  NSError* config_error = [manager_ configureAudioSession];
  EXPECT_NSEQ(config_error, nil);

  [manager_ disconnect];
  [manager_ disconnect];

  AVAudioSession* session = [AVAudioSession sharedInstance];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return ![session.category
            isEqualToString:AVAudioSessionCategoryPlayAndRecord];
      }));
}

// Test that configureAudioSessionWithCompletion: returns a cancelled error if
// invoked after the manager has been disconnected.
TEST_F(TTCAudioSessionManagerTest, TestConfigureCancelledWhenDisconnected) {
  [manager_ disconnect];

  base::test::TestFuture<NSError*> future;
  [manager_ configureAudioSessionWithCompletion:base::CallbackToBlock(
                                                    future.GetCallback())];

  NSError* error = future.Take();
  ASSERT_NSNE(error, nil);
  EXPECT_NSEQ(error.domain, kTTCAudioSessionManagerErrorDomain);
  EXPECT_EQ(error.code, static_cast<NSInteger>(
                            TTCAudioSessionManagerErrorCode::kCancelled));
}

// Test that configureAudioSessionWithCompletion: returns a cancelled error if
// the manager is deallocated before the background task completes.
TEST_F(TTCAudioSessionManagerTest,
       TestConfigureCancelledWhenManagerDeallocated) {
  base::test::TestFuture<NSError*> future;
  @autoreleasepool {
    TTCAudioSessionManager* ephemeral_manager =
        [[TTCAudioSessionManager alloc] init];
    [ephemeral_manager
        configureAudioSessionWithCompletion:base::CallbackToBlock(
                                                future.GetCallback())];
    ephemeral_manager = nil;
  }

  NSError* error = future.Take();
  ASSERT_NSNE(error, nil);
  EXPECT_NSEQ(error.domain, kTTCAudioSessionManagerErrorDomain);
  EXPECT_EQ(error.code, static_cast<NSInteger>(
                            TTCAudioSessionManagerErrorCode::kCancelled));
}

}  // namespace
