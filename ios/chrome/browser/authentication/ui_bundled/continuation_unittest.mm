// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "testing/platform_test.h"

class ContinuationTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ContinuationTest, DoNothingContinuation) {
  ChangeProfileContinuation continuation = DoNothingContinuation();

  base::RunLoop run_loop;
  bool closure_called = false;
  base::OnceClosure closure = base::BindOnce(
      [](bool* closure_called) { *closure_called = true; }, &closure_called);
  SceneState* scene_state = nil;
  std::move(continuation)
      .Run(scene_state, std::move(closure).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(closure_called);
}

TEST_F(ContinuationTest, DoNothingContinuationProvider) {
  ChangeProfileContinuationProvider provider = DoNothingContinuationProvider();
  ChangeProfileContinuation continuation = provider.Run();

  base::RunLoop run_loop;
  bool closure_called = false;
  base::OnceClosure closure = base::BindOnce(
      [](bool* closure_called) { *closure_called = true; }, &closure_called);
  SceneState* scene_state = nil;
  std::move(continuation)
      .Run(scene_state, std::move(closure).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(closure_called);
}
