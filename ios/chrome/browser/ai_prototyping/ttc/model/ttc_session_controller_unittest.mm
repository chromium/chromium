// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/run_until.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake observer conforming to TTCSessionControllerObserver for unit testing.
@interface FakeTTCSessionControllerObserver
    : NSObject <TTCSessionControllerObserver>

@property(nonatomic, assign) NSInteger lifecycleChangeCount;
@property(nonatomic, assign) TTCSessionLifecycle lastLifecycle;
@property(nonatomic, assign) NSInteger audioLevelUpdateCount;
@property(nonatomic, assign) float lastAudioLevel;
@property(nonatomic, assign) NSInteger errorCount;
@property(nonatomic, strong) NSError* lastError;

@end

@implementation FakeTTCSessionControllerObserver

- (void)sessionController:(TTCSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle {
  _lifecycleChangeCount++;
  _lastLifecycle = lifecycle;
}

- (void)sessionController:(TTCSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel {
  _audioLevelUpdateCount++;
  _lastAudioLevel = audioLevel;
}

- (void)sessionController:(TTCSessionController*)controller
         didFailWithError:(NSError*)error {
  _errorCount++;
  _lastError = error;
}

@end

class TTCSessionControllerTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests that a controller initializes in kInitializing state.
TEST_F(TTCSessionControllerTest, TestInitialLifecycleIsInitializing) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  ASSERT_TRUE(controller != nil);
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kInitializing);
  [controller disconnect];
}

// Tests that onSessionInitialized transitions the lifecycle to kLive and
// notifies observers.
TEST_F(TTCSessionControllerTest, TestSessionInitializedTransitionsToLive) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);

  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycleChangeCount, 1);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kLive);
  [controller disconnect];
}

// Tests that stopSession transitions to kFinished and disconnects.
TEST_F(TTCSessionControllerTest, TestStopSessionTransitionsToFinished) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);

  [controller stopSession];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycleChangeCount, 2);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);
}

// Tests that disconnect transitions lifecycle to kFinished and notifies
// observers.
TEST_F(TTCSessionControllerTest, TestObserverNotifiedOnDisconnect) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);

  [controller disconnect];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycleChangeCount, 1);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);
}

// Tests that UIApplicationDidEnterBackgroundNotification stops the session,
// moves lifecycle to kFinished, and notifies observers.
TEST_F(TTCSessionControllerTest, TestBackgroundNotificationStopsSession) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kLive);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];

  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);

  [controller disconnect];
}

// Tests that userAudioLevelDidUpdate forwards the level to observers.
TEST_F(TTCSessionControllerTest, TestAudioLevelUpdateForwardedToObserver) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller userAudioLevelDidUpdate:0.65f];
  EXPECT_EQ(observer.audioLevelUpdateCount, 1);
  EXPECT_FLOAT_EQ(observer.lastAudioLevel, 0.65f);
  [controller disconnect];
}

// Tests that failWithError forwards the error to observers.
TEST_F(TTCSessionControllerTest, TestFailWithErrorForwardedToObserver) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  NSError* error = [NSError errorWithDomain:@"org.chromium.ttc"
                                       code:42
                                   userInfo:nil];
  [controller failWithError:error];
  EXPECT_EQ(observer.errorCount, 1);
  EXPECT_EQ(observer.lastError, error);
  [controller disconnect];
}

// Tests that removing an observer stops notifications.
TEST_F(TTCSessionControllerTest, TestRemoveObserverStopsNotifications) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];
  [controller removeObserver:observer];

  [controller onSessionInitialized];
  [controller userAudioLevelDidUpdate:0.5f];
  EXPECT_EQ(observer.lifecycleChangeCount, 0);
  EXPECT_EQ(observer.audioLevelUpdateCount, 0);
  [controller disconnect];
}

// Tests that disconnect cleanly unregisters background notifications.
TEST_F(TTCSessionControllerTest,
       TestDisconnectCleanlyUnregistersBackgroundNotification) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  [controller startSession];
  [controller disconnect];

  // Posting background notification after disconnect should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
}

// Tests that adding or removing a nil observer is a safe no-op and does not
// crash.
TEST_F(TTCSessionControllerTest, TestAddObserverNilDoesNotCrash) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  [controller addObserver:nil];
  [controller removeObserver:nil];
  [controller disconnect];
}

// Tests that onSessionInitialized does not resurrect a session that has already
// finished.
TEST_F(TTCSessionControllerTest,
       TestLateOnSessionInitializedDoesNotResurrectFinishedSession) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  [controller startSession];
  [controller stopSession];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);

  // Late initialization after session termination must not resurrect to kLive.
  [controller onSessionInitialized];
  EXPECT_EQ(controller.lifecycle, TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lastLifecycle, TTCSessionLifecycle::kFinished);

  [controller disconnect];
}

// Tests that audio level updates from background threads are safely delivered
// on the main thread.
TEST_F(TTCSessionControllerTest, TestAudioLevelDispatchedFromBackgroundThread) {
  TTCSessionController* controller = [[TTCSessionController alloc] init];
  FakeTTCSessionControllerObserver* observer =
      [[FakeTTCSessionControllerObserver alloc] init];
  [controller addObserver:observer];

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                 ^{
                   [controller userAudioLevelDidUpdate:0.8f];
                 });

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.audioLevelUpdateCount == 1; }));
  EXPECT_EQ(observer.audioLevelUpdateCount, 1);
  EXPECT_FLOAT_EQ(observer.lastAudioLevel, 0.8f);

  [controller disconnect];
}
