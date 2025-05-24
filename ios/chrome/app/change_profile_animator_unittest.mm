// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/change_profile_animator.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns a closure that will set `called` to `true` when invoked.
base::OnceClosure RecordWhenCalled(bool* called) {
  return base::BindOnce([](bool* called) { *called = true; }, called);
}

// Returns a continuation that call a closure when invoked.
ChangeProfileContinuation ContinuationFromClosure(base::OnceClosure closure) {
  return base::BindOnce(
      [](base::OnceClosure done, SceneState*, base::OnceClosure next) {
        std::move(done).Run();
        std::move(next).Run();
      },
      std::move(closure));
}

}  // namespace

// NOTE for the whole file:
//
// The code wants to test that the ChangeProfileAnimator extends its lifetime
// to stay alive as long as the wait for the initialisation is not over, then
// ensure it is deallocated before calling the continuation.
//
// To detect this, the test cases create a weak pointer to the animator, and
// wrap all calls to method that could result in deallocation of the object
// in @autoreleasepool { ... }.
//
// The goal is to ensure that even if the compiler inserts retain/autorelease
// to manage the lifetime of the object, those would not extend it past the
// @autoreleasepool { ... } blocks. This allow to detect if the animator is
// correctly deallocated when expected and only when expected (if only the
// calls where the deallocation is expected were to be wrapped, the bug could
// be missed if the compiler used retain/autorelease).
//
// So please keep those @autoreleasepool { ... } in the test case around the
// method calls that may cause the wait to complete.

class ChangeProfileAnimatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state_;
    [profile_state_ sceneStateConnected:scene_state_];
  }

  void TearDown() override {
    DisconnectSceneState();
    profile_state_ = nil;
    PlatformTest::TearDown();
  }

  ProfileState* profile_state() { return profile_state_; }

  SceneState* scene_state() { return scene_state_; }

  void DisconnectSceneState() {
    scene_state_.activationLevel = SceneActivationLevelDisconnected;
    scene_state_.profileState = nil;
    scene_state_ = nil;
  }

 private:
  base::test::TaskEnvironment task_env_;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
};

// Check that ChangeProfileAnimator is strongly retained by the SceneState
// until the continuation is called, and that it calls the continuation as
// soon as the SceneState and ProfileState are both ready.
TEST_F(ChangeProfileAnimatorTest, waitForSceneState_ProfileReadyBeforeScene) {
  base::RunLoop run_loop;
  bool continuation_called = false;
  __weak ChangeProfileAnimator* weak_animator = nil;
  @autoreleasepool {
    ChangeProfileAnimator* animator =
        [[ChangeProfileAnimator alloc] initWithWindow:nil];

    [animator waitForSceneState:scene_state()
               toReachInitStage:ProfileInitStage::kUIReady
                   continuation:ContinuationFromClosure(
                                    RecordWhenCalled(&continuation_called)
                                        .Then(run_loop.QuitClosure()))];

    weak_animator = animator;
    animator = nil;
  }

  // The animator should be kept alive by the SceneState, the continuation
  // should not yet have been called.
  ASSERT_TRUE(weak_animator);
  ASSERT_FALSE(continuation_called);

  // Simulate the ProfileState initialisation progressing to kUIReady. The
  // continuation is not called yet.
  @autoreleasepool {
    SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);
  }

  EXPECT_TRUE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Simulate the SceneState reaches the foreground. The animator posts the
  // continuation to be invoked soon, and then is deallocated.
  @autoreleasepool {
    scene_state().activationLevel = SceneActivationLevelForegroundInactive;
    scene_state().UIEnabled = YES;
  }

  EXPECT_FALSE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Wait until the posted continuation is invoked.
  run_loop.Run();

  EXPECT_FALSE(weak_animator);
  EXPECT_TRUE(continuation_called);
}

// Check that ChangeProfileAnimator is strongly retained by the SceneState
// until the continuation is called, and that it calls the continuation as
// soon as the SceneState and ProfileState are both ready.
TEST_F(ChangeProfileAnimatorTest, waitForSceneState_SceneReadyBeforeProfile) {
  base::RunLoop run_loop;
  bool continuation_called = false;
  __weak ChangeProfileAnimator* weak_animator = nil;
  @autoreleasepool {
    ChangeProfileAnimator* animator =
        [[ChangeProfileAnimator alloc] initWithWindow:nil];

    [animator waitForSceneState:scene_state()
               toReachInitStage:ProfileInitStage::kUIReady
                   continuation:ContinuationFromClosure(
                                    RecordWhenCalled(&continuation_called)
                                        .Then(run_loop.QuitClosure()))];

    weak_animator = animator;
    animator = nil;
  }

  // The animator should be kept alive by the SceneState, the continuation
  // should not yet have been called.
  ASSERT_TRUE(weak_animator);
  ASSERT_FALSE(continuation_called);

  // Simulate the ProfileState initialisation progressing to kPrepareUI. The
  // continuation is not called yet.
  @autoreleasepool {
    SetProfileStateInitStage(profile_state(), ProfileInitStage::kPrepareUI);
  }

  EXPECT_TRUE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Simulate the SceneState reaches the foreground. The continuation is not
  // called yet.
  @autoreleasepool {
    scene_state().activationLevel = SceneActivationLevelForegroundInactive;
    scene_state().UIEnabled = YES;
  }

  EXPECT_TRUE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Simulate the ProfileState initialisation progressing to kUIReady. The
  // animator post the continuation to be invoked soon, and then is
  // deallocated.
  @autoreleasepool {
    SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);
  }

  EXPECT_FALSE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Wait until the posted continuation is invoked.
  run_loop.Run();

  EXPECT_FALSE(weak_animator);
  EXPECT_TRUE(continuation_called);
}

// Check that ChangeProfileAnimator is strongly retained by the SceneState
// until the continuation is called, and that it calls the continuation as
// soon as the SceneState and ProfileState are both ready.
TEST_F(ChangeProfileAnimatorTest, waitForSceneState_ProfileAlreadyReady) {
  // Simulate the ProfileState initialisation progressing to kFinal. This
  // correspond to the case when switching to a profile that is already
  // used for another Scene.
  @autoreleasepool {
    SetProfileStateInitStage(profile_state(), ProfileInitStage::kFinal);
  }

  base::RunLoop run_loop;
  bool continuation_called = false;
  __weak ChangeProfileAnimator* weak_animator = nil;
  @autoreleasepool {
    ChangeProfileAnimator* animator =
        [[ChangeProfileAnimator alloc] initWithWindow:nil];

    [animator waitForSceneState:scene_state()
               toReachInitStage:ProfileInitStage::kUIReady
                   continuation:ContinuationFromClosure(
                                    RecordWhenCalled(&continuation_called)
                                        .Then(run_loop.QuitClosure()))];

    weak_animator = animator;
    animator = nil;
  }

  // The animator should be kept alive by the SceneState, the continuation
  // should not yet have been called.
  ASSERT_TRUE(weak_animator);
  ASSERT_FALSE(continuation_called);

  // Simulate the SceneState reaches the foreground. The animator post the
  // continuation to be invoked soon, and then is deallocated.
  @autoreleasepool {
    scene_state().activationLevel = SceneActivationLevelForegroundInactive;
    scene_state().UIEnabled = YES;
  }

  EXPECT_FALSE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Wait until the posted continuation is invoked.
  run_loop.Run();

  EXPECT_FALSE(weak_animator);
  EXPECT_TRUE(continuation_called);
}

// Check that ChangeProfileAnimator is dropped and the continuation is not
// called if the Scene is deallocated.
TEST_F(ChangeProfileAnimatorTest, waitForSceneState_SceneDeallocated) {
  // Simulate the ProfileState initialisation progressing to kFinal. This
  // correspond to the case when switching to a profile that is already
  // used for another Scene.
  @autoreleasepool {
    SetProfileStateInitStage(profile_state(), ProfileInitStage::kFinal);
  }

  base::RunLoop run_loop;
  bool continuation_called = false;
  __weak ChangeProfileAnimator* weak_animator = nil;
  @autoreleasepool {
    ChangeProfileAnimator* animator =
        [[ChangeProfileAnimator alloc] initWithWindow:nil];

    [animator waitForSceneState:scene_state()
               toReachInitStage:ProfileInitStage::kUIReady
                   continuation:ContinuationFromClosure(
                                    RecordWhenCalled(&continuation_called)
                                        .Then(run_loop.QuitClosure()))];

    weak_animator = animator;
    animator = nil;
  }

  // The animator should be kept alive by the SceneState, the continuation
  // should not yet have been called.
  ASSERT_TRUE(weak_animator);
  ASSERT_FALSE(continuation_called);

  // Simulate the SceneState is deallocated while waiting for initialisation.
  @autoreleasepool {
    DisconnectSceneState();
  }

  EXPECT_FALSE(weak_animator);
  EXPECT_FALSE(continuation_called);

  // Ensure the continuation is not invoked. Post a callback with a timeout
  // to avoid failing the test due to a timeout.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));

  run_loop.Run();

  EXPECT_FALSE(weak_animator);
  EXPECT_FALSE(continuation_called);
}
