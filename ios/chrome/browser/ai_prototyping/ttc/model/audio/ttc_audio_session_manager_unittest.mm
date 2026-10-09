// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/callback_helpers.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test fake conforming to TTCAudioSessionManagerDelegate for verifying
// lifecycle, route changes, reconfigurations, and interruption notifications.
@interface FakeTTCAudioSessionManagerDelegate
    : NSObject <TTCAudioSessionManagerDelegate>
@property(nonatomic, assign) BOOL interruptionBeganCalled;
@property(nonatomic, assign) BOOL interruptionEndedCalled;
@property(nonatomic, assign) BOOL didChangeRouteCalled;
@property(nonatomic, assign) BOOL didRequireReconfigurationCalled;
@property(nonatomic, copy) NSString* lastRouteDescription;
@property(nonatomic, assign) BOOL lastHasHardwareAEC;
@property(nonatomic, weak) TTCAudioSessionManager* lastDeliveredManager;
@property(nonatomic, assign) BOOL shouldResumeValue;
@property(nonatomic, copy) void (^onInterruptionBegan)(void);
@property(nonatomic, copy) void (^onInterruptionEnded)(BOOL shouldResume);
@property(nonatomic, copy) void (^onRouteChange)
    (NSString* routeDescription, BOOL hasHardwareAEC);
@property(nonatomic, copy) void (^onReconfigurationRequired)(void);
@end

@implementation FakeTTCAudioSessionManagerDelegate

- (void)audioSessionManagerDidBeginInterruption:
    (TTCAudioSessionManager*)manager {
  _lastDeliveredManager = manager;
  _interruptionBeganCalled = YES;
  if (_onInterruptionBegan) {
    auto block = _onInterruptionBegan;
    _onInterruptionBegan = nil;
    block();
  }
}

- (void)audioSessionManager:(TTCAudioSessionManager*)manager
    didEndInterruptionWithShouldResume:(BOOL)shouldResume {
  _lastDeliveredManager = manager;
  _interruptionEndedCalled = YES;
  _shouldResumeValue = shouldResume;
  if (_onInterruptionEnded) {
    auto block = _onInterruptionEnded;
    _onInterruptionEnded = nil;
    block(shouldResume);
  }
}

- (void)audioSessionManager:(TTCAudioSessionManager*)manager
    didChangeRouteDescription:(NSString*)routeDescription
               hasHardwareAEC:(BOOL)hasAEC {
  _lastDeliveredManager = manager;
  _didChangeRouteCalled = YES;
  _lastRouteDescription = [routeDescription copy];
  _lastHasHardwareAEC = hasAEC;
  if (_onRouteChange) {
    auto block = _onRouteChange;
    _onRouteChange = nil;
    block(routeDescription, hasAEC);
  }
}

- (void)audioSessionManagerDidRequireEngineReconfiguration:
    (TTCAudioSessionManager*)manager {
  _lastDeliveredManager = manager;
  _didRequireReconfigurationCalled = YES;
  if (_onReconfigurationRequired) {
    auto block = _onReconfigurationRequired;
    _onReconfigurationRequired = nil;
    block();
  }
}

@end

namespace {

class TTCAudioSessionManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    AVAudioSession* session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategorySoloAmbient
                    mode:AVAudioSessionModeDefault
                 options:0
                   error:nil];
    manager_ = [[TTCAudioSessionManager alloc] init];
    delegate_ = [[FakeTTCAudioSessionManagerDelegate alloc] init];
    manager_.delegate = delegate_;
  }

  void TearDown() override {
    @autoreleasepool {
      [manager_ disconnect];
      manager_ = nil;
      delegate_ = nil;
    }
    AVAudioSession* session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategorySoloAmbient
                    mode:AVAudioSessionModeDefault
                 options:0
                   error:nil];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  TTCAudioSessionManager* manager_ = nil;
  FakeTTCAudioSessionManagerDelegate* delegate_ = nil;
};

// Test that configureAudioSession configures PlayAndRecord and VideoChat mode
// synchronously on the UI thread, and notifies delegate of initial route.
TEST_F(TTCAudioSessionManagerTest, TestConfigureAudioSessionSynchronous) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  NSError* error = [manager_ configureAudioSession];
  EXPECT_NSEQ(error, nil);

  AVAudioSession* session = [AVAudioSession sharedInstance];
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);
  EXPECT_NSEQ(session.mode, AVAudioSessionModeVideoChat);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  ASSERT_NE(route_desc, nil);
}

// Test that configureAudioSessionWithCompletion: configures the session
// asynchronously on the task runner and invokes completion on the UI thread.
TEST_F(TTCAudioSessionManagerTest, TestConfigureAudioSessionWithCompletion) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  base::test::TestFuture<NSError*> future;
  [manager_ configureAudioSessionWithCompletion:base::CallbackToBlock(
                                                    future.GetCallback())];

  NSError* error = future.Take();
  EXPECT_NSEQ(error, nil);

  AVAudioSession* session = [AVAudioSession sharedInstance];
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);
  EXPECT_NSEQ(session.mode, AVAudioSessionModeVideoChat);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  ASSERT_NE(route_desc, nil);
}

// Test that hasHardwareAEC matches the active input port's hardware voice call
// processing capability.
TEST_F(TTCAudioSessionManagerTest, TestHasHardwareAEC) {
  AVAudioSessionPortDescription* input_port =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  EXPECT_EQ(manager_.hasHardwareAEC, input_port.hasHardwareVoiceCallProcessing);
}

// Test that isOutputRoutedToSpeaker returns YES when no external or earpiece
// output port is active.
TEST_F(TTCAudioSessionManagerTest, TestIsOutputRoutedToSpeaker) {
  EXPECT_TRUE(manager_.isOutputRoutedToSpeaker);
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

// Test that an interruption began notification is dispatched to the delegate on
// the UI thread.
TEST_F(TTCAudioSessionManagerTest, TestInterruptionBeganNotification) {
  base::test::TestFuture<void> began_future;
  delegate_.onInterruptionBegan =
      base::CallbackToBlock(began_future.GetCallback());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionInterruptionNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{
                    AVAudioSessionInterruptionTypeKey :
                        @(AVAudioSessionInterruptionTypeBegan)
                  }];

  EXPECT_TRUE(began_future.Wait());
  EXPECT_TRUE(delegate_.interruptionBeganCalled);
  EXPECT_FALSE(delegate_.interruptionEndedCalled);
  EXPECT_EQ(delegate_.lastDeliveredManager, manager_);
}

// Test that an interruption ended notification with shouldResume=YES notifies
// the delegate with shouldResume=YES.
TEST_F(TTCAudioSessionManagerTest,
       TestInterruptionEndedNotificationWithShouldResume) {
  base::test::TestFuture<bool> ended_future;
  delegate_.onInterruptionEnded =
      base::CallbackToBlock(ended_future.GetCallback());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionInterruptionNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{
                    AVAudioSessionInterruptionTypeKey :
                        @(AVAudioSessionInterruptionTypeEnded),
                    AVAudioSessionInterruptionOptionKey :
                        @(AVAudioSessionInterruptionOptionShouldResume)
                  }];

  EXPECT_TRUE(ended_future.Wait());
  EXPECT_TRUE(delegate_.interruptionEndedCalled);
  EXPECT_FALSE(delegate_.interruptionBeganCalled);
  EXPECT_TRUE(ended_future.Get());
  EXPECT_EQ(delegate_.lastDeliveredManager, manager_);
}

// Test that an interruption ended notification without shouldResume notifies
// the delegate with shouldResume=NO.
TEST_F(TTCAudioSessionManagerTest,
       TestInterruptionEndedNotificationWithoutResume) {
  base::test::TestFuture<bool> ended_future;
  delegate_.onInterruptionEnded =
      base::CallbackToBlock(ended_future.GetCallback());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionInterruptionNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{
                    AVAudioSessionInterruptionTypeKey :
                        @(AVAudioSessionInterruptionTypeEnded),
                    AVAudioSessionInterruptionOptionKey : @0
                  }];

  EXPECT_TRUE(ended_future.Wait());
  EXPECT_TRUE(delegate_.interruptionEndedCalled);
  EXPECT_FALSE(delegate_.interruptionBeganCalled);
  EXPECT_FALSE(ended_future.Get());
  EXPECT_EQ(delegate_.lastDeliveredManager, manager_);
}

// Test that an interruption notification with empty userInfo is safely ignored.
TEST_F(TTCAudioSessionManagerTest,
       TestMalformedInterruptionNotificationIgnored) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionInterruptionNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{}];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.interruptionBeganCalled);
  EXPECT_FALSE(delegate_.interruptionEndedCalled);
}

// Test that disconnect removes notification observers and clears delegate so no
// callbacks are delivered afterwards.
TEST_F(TTCAudioSessionManagerTest, TestDisconnectRemovesInterruptionObserver) {
  [manager_ disconnect];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionInterruptionNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{
                    AVAudioSessionInterruptionTypeKey :
                        @(AVAudioSessionInterruptionTypeBegan)
                  }];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.interruptionBeganCalled);
}

// Test that disconnect can be called safely multiple times without errors,
// and ensures the prior session state is restored.
TEST_F(TTCAudioSessionManagerTest, TestDisconnectIdempotence) {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* prime_error = nil;
  [session setCategory:AVAudioSessionCategorySoloAmbient
                  mode:AVAudioSessionModeDefault
               options:0
                 error:&prime_error];
  ASSERT_NSEQ(prime_error, nil);

  NSError* config_error = [manager_ configureAudioSession];
  EXPECT_NSEQ(config_error, nil);
  EXPECT_NSEQ(session.category, AVAudioSessionCategoryPlayAndRecord);

  [manager_ disconnect];
  [manager_ disconnect];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return [session.category
            isEqualToString:AVAudioSessionCategorySoloAmbient];
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

// Test that malformed route change notifications (missing userInfo or reason)
// are safely ignored without dispatching delegate callbacks.
TEST_F(TTCAudioSessionManagerTest,
       TestMalformedRouteChangeNotificationIgnored) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:nil];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:@{}];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_FALSE(delegate_.didChangeRouteCalled);
}

// Test that AVAudioSessionRouteChangeNotification with Override reason
// updates route description without requesting engine reconfiguration.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithOverrideDoesNotRequestReconfiguration) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonOverride)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_NSNE(route_desc, nil);
}

// Test that AVAudioSessionRouteChangeNotification with CategoryChange reason
// updates route description without requesting engine reconfiguration.
TEST_F(
    TTCAudioSessionManagerTest,
    TestRouteChangeNotificationWithCategoryChangeDoesNotRequestReconfiguration) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonCategoryChange)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_NSNE(route_desc, nil);
}

// Test that AVAudioSessionRouteChangeNotification with NewDeviceAvailable
// notifies delegate of route change and requests engine reconfiguration.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithNewDeviceAvailable) {
  base::test::TestFuture<void> reconfig_future;
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onReconfigurationRequired =
      base::CallbackToBlock(reconfig_future.GetCallback());
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonNewDeviceAvailable)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  EXPECT_TRUE(reconfig_future.Wait());
  EXPECT_TRUE(route_future.Wait());
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
}

// Test that AVAudioSessionRouteChangeNotification with OldDeviceUnavailable
// notifies delegate of route change and requests engine reconfiguration.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithOldDeviceUnavailable) {
  base::test::TestFuture<void> reconfig_future;
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onReconfigurationRequired =
      base::CallbackToBlock(reconfig_future.GetCallback());
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonOldDeviceUnavailable)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  EXPECT_TRUE(reconfig_future.Wait());
  EXPECT_TRUE(route_future.Wait());
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
}

// Test that AVAudioSessionRouteChangeNotification with RouteConfigurationChange
// updates route description without requesting engine reconfiguration.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithRouteConfigurationChange) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonRouteConfigurationChange)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_NSNE(route_desc, nil);
}

// Test that disconnect unregisters route change observers and ensures no
// callbacks are delivered afterwards.
TEST_F(TTCAudioSessionManagerTest, TestDisconnectRemovesRouteChangeObserver) {
  [manager_ disconnect];

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonNewDeviceAvailable)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_FALSE(delegate_.didChangeRouteCalled);
}

}  // namespace
