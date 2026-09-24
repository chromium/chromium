// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/callback_helpers.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager_delegate.h"
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

// Test that configureAudioSession configures PlayAndRecord and appropriate mode
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

// Test that setting output destination synchronously updates outputDestination
// and notifies delegate of route and reconfiguration changes.
TEST_F(TTCAudioSessionManagerTest, TestSetOutputDestination) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  NSError* error = nil;
  BOOL success =
      [manager_ setOutputDestination:TTCAudioOutputDestination::kEarpiece
                               error:&error];
  EXPECT_TRUE(success);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kEarpiece);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
  ASSERT_NE(route_desc, nil);
}

// Test that asynchronously setting output destination updates outputDestination
// and notifies delegate.
TEST_F(TTCAudioSessionManagerTest, TestSetOutputDestinationAsync) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  base::test::TestFuture<BOOL, NSError*> future;
  [manager_ setOutputDestination:TTCAudioOutputDestination::kEarpiece
                      completion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_NSEQ(error, nil);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kEarpiece);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
  ASSERT_NE(route_desc, nil);
}

// Test that setOutputOverriddenToSpeaker transitions between speaker and
// natural routing.
TEST_F(TTCAudioSessionManagerTest, TestSetOutputOverriddenToSpeaker) {
  NSError* error = nil;
  BOOL success = [manager_ setOutputOverriddenToSpeaker:YES error:&error];
  EXPECT_TRUE(success);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);

  // Transitioning to NO should route to earpiece (when no external device is
  // connected).
  success = [manager_ setOutputOverriddenToSpeaker:NO error:&error];
  EXPECT_TRUE(success);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kEarpiece);
}

// Test that asynchronously overriding output to speaker updates destination.
TEST_F(TTCAudioSessionManagerTest, TestSetOutputOverriddenToSpeakerAsync) {
  base::test::TestFuture<BOOL, NSError*> future;
  [manager_
      setOutputOverriddenToSpeaker:YES
                        completion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_NSEQ(error, nil);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);

  base::test::TestFuture<BOOL, NSError*> restore_future;
  [manager_ setOutputOverriddenToSpeaker:NO
                              completion:base::CallbackToBlock(
                                             restore_future.GetCallback())];
  auto [restore_success, restore_error] = restore_future.Take();
  EXPECT_TRUE(restore_success);
  EXPECT_NSEQ(restore_error, nil);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kEarpiece);
}

// Test that setPreferredInput succeeds and requests reconfiguration.
TEST_F(TTCAudioSessionManagerTest, TestSetPreferredInput) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  NSError* error = nil;
  BOOL success = [manager_ setPreferredInput:nil error:&error];
  EXPECT_TRUE(success);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
}

// Test that asynchronously setting preferred input succeeds and notifies
// delegate.
TEST_F(TTCAudioSessionManagerTest, TestSetPreferredInputAsync) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange =
      base::CallbackToBlock(route_future.GetRepeatingCallback());

  base::test::TestFuture<BOOL, NSError*> future;
  [manager_ setPreferredInput:nil
                   completion:base::CallbackToBlock(future.GetCallback())];

  auto [success, error] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_NSEQ(error, nil);

  auto [route_desc, has_aec] = route_future.Take();
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
}

// Test that availableInputs and preferredInput return valid collections and
// properties.
TEST_F(TTCAudioSessionManagerTest, TestPortProperties) {
  NSArray<AVAudioSessionPortDescription*>* inputs = manager_.availableInputs;
  ASSERT_NE(inputs, nil);

  // preferredInput is nil by default when using system default routing.
  EXPECT_NSEQ(manager_.preferredInput, nil);

  // Initial destination is either kSpeaker or kExternal.
  EXPECT_TRUE(
      manager_.outputDestination == TTCAudioOutputDestination::kSpeaker ||
      manager_.outputDestination == TTCAudioOutputDestination::kExternal);
}

// Test that routing and input methods return cancellation errors when
// disconnected.
TEST_F(TTCAudioSessionManagerTest, TestRoutingCancelledWhenDisconnected) {
  [manager_ disconnect];

  NSError* error = nil;
  BOOL sync_dest_success =
      [manager_ setOutputDestination:TTCAudioOutputDestination::kSpeaker
                               error:&error];
  EXPECT_FALSE(sync_dest_success);
  ASSERT_NSNE(error, nil);
  EXPECT_EQ(error.code, static_cast<NSInteger>(
                            TTCAudioSessionManagerErrorCode::kCancelled));

  base::test::TestFuture<BOOL, NSError*> async_dest_future;
  [manager_ setOutputDestination:TTCAudioOutputDestination::kSpeaker
                      completion:base::CallbackToBlock(
                                     async_dest_future.GetCallback())];
  auto [async_dest_success, async_dest_error] = async_dest_future.Take();
  EXPECT_FALSE(async_dest_success);
  ASSERT_NSNE(async_dest_error, nil);
  EXPECT_EQ(
      async_dest_error.code,
      static_cast<NSInteger>(TTCAudioSessionManagerErrorCode::kCancelled));

  NSError* input_error = nil;
  BOOL sync_input_success = [manager_ setPreferredInput:nil error:&input_error];
  EXPECT_FALSE(sync_input_success);
  ASSERT_NSNE(input_error, nil);
  EXPECT_EQ(input_error.code, static_cast<NSInteger>(
                                  TTCAudioSessionManagerErrorCode::kCancelled));

  base::test::TestFuture<BOOL, NSError*> async_input_future;
  [manager_ setPreferredInput:nil
                   completion:base::CallbackToBlock(
                                  async_input_future.GetCallback())];
  auto [async_input_success, async_input_error] = async_input_future.Take();
  EXPECT_FALSE(async_input_success);
  ASSERT_NSNE(async_input_error, nil);
  EXPECT_EQ(
      async_input_error.code,
      static_cast<NSInteger>(TTCAudioSessionManagerErrorCode::kCancelled));
}

// Test that rapid out-of-order destination changes return cancellation error
// for superseded requests.
TEST_F(TTCAudioSessionManagerTest, TestSupersededDestinationChange) {
  base::test::TestFuture<BOOL, NSError*> first_future;
  base::test::TestFuture<BOOL, NSError*> second_future;

  [manager_
      setOutputDestination:TTCAudioOutputDestination::kEarpiece
                completion:base::CallbackToBlock(first_future.GetCallback())];
  [manager_
      setOutputDestination:TTCAudioOutputDestination::kSpeaker
                completion:base::CallbackToBlock(second_future.GetCallback())];

  auto [first_success, first_error] = first_future.Take();
  auto [second_success, second_error] = second_future.Take();

  EXPECT_FALSE(first_success);
  ASSERT_NSNE(first_error, nil);
  EXPECT_EQ(first_error.code, static_cast<NSInteger>(
                                  TTCAudioSessionManagerErrorCode::kCancelled));

  EXPECT_TRUE(second_success);
  EXPECT_NSEQ(second_error, nil);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);
}

// Test that hasHardwareAEC matches the active input port's hardware voice call
// processing capability.
TEST_F(TTCAudioSessionManagerTest, TestHasHardwareAEC) {
  AVAudioSessionPortDescription* input_port =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  EXPECT_EQ(manager_.hasHardwareAEC, input_port.hasHardwareVoiceCallProcessing);
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

// Test that AVAudioEngineConfigurationChangeNotification requests engine
// reconfiguration on the delegate.
TEST_F(TTCAudioSessionManagerTest,
       TestEngineConfigurationChangeNotificationRequestsReconfiguration) {
  base::test::TestFuture<void> reconfig_future;
  delegate_.onReconfigurationRequired =
      base::CallbackToBlock(reconfig_future.GetCallback());

  @autoreleasepool {
    AVAudioEngine* engine = [[AVAudioEngine alloc] init];
    [manager_ registerNotificationObserversWithAudioEngine:engine];

    [[NSNotificationCenter defaultCenter]
        postNotificationName:AVAudioEngineConfigurationChangeNotification
                      object:engine];
  }

  EXPECT_TRUE(reconfig_future.Wait());
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
}

// Test that AVAudioEngineConfigurationChangeNotification is ignored after
// the manager has been disconnected.
TEST_F(TTCAudioSessionManagerTest,
       TestEngineConfigurationChangeNotificationIgnoredWhenDisconnected) {
  AVAudioEngine* engine = [[AVAudioEngine alloc] init];
  [manager_ registerNotificationObserversWithAudioEngine:engine];
  [manager_ disconnect];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioEngineConfigurationChangeNotification
                    object:engine];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
}

// Test that AVAudioEngineConfigurationChangeNotification is not observed when
// registering with a nil engine.
TEST_F(TTCAudioSessionManagerTest,
       TestEngineConfigurationChangeNotificationWithNilEngine) {
  [manager_ registerNotificationObserversWithAudioEngine:nil];

  AVAudioEngine* engine = [[AVAudioEngine alloc] init];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioEngineConfigurationChangeNotification
                    object:engine];

  base::test::TestFuture<void> flush_future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());

  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
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
// updates route description without forcing external output if no external
// output port is connected.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithNewDeviceAvailableWithoutExternalOutput) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonNewDeviceAvailable)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  EXPECT_TRUE(route_future.Wait());
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_FALSE(delegate_.didRequireReconfigurationCalled);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);
}

// Test that AVAudioSessionRouteChangeNotification with OldDeviceUnavailable
// defaults to speaker when no external devices remain connected.
TEST_F(TTCAudioSessionManagerTest,
       TestRouteChangeNotificationWithOldDeviceUnavailableDefaultsToSpeaker) {
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonOldDeviceUnavailable)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  EXPECT_TRUE(route_future.Wait());
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);
}

// Test that AVAudioSessionRouteChangeNotification with RouteConfigurationChange
// requests engine reconfiguration and synchronizes outputDestination to the
// active session route.
TEST_F(
    TTCAudioSessionManagerTest,
    TestRouteChangeNotificationWithRouteConfigurationChangeSynchronizesDestination) {
  base::test::TestFuture<void> reconfig_future;
  base::test::TestFuture<NSString*, BOOL> route_future;
  delegate_.onReconfigurationRequired =
      base::CallbackToBlock(reconfig_future.GetCallback());
  delegate_.onRouteChange = base::CallbackToBlock(route_future.GetCallback());

  NSDictionary* userInfo = @{
    AVAudioSessionRouteChangeReasonKey :
        @(AVAudioSessionRouteChangeReasonRouteConfigurationChange)
  };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:AVAudioSessionRouteChangeNotification
                    object:[AVAudioSession sharedInstance]
                  userInfo:userInfo];

  EXPECT_TRUE(reconfig_future.Wait());
  EXPECT_TRUE(route_future.Wait());
  EXPECT_TRUE(delegate_.didRequireReconfigurationCalled);
  EXPECT_TRUE(delegate_.didChangeRouteCalled);
  EXPECT_EQ(manager_.outputDestination, TTCAudioOutputDestination::kSpeaker);
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
