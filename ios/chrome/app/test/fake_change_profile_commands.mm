// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/test/fake_change_profile_commands.h"

#import <optional>
#import <string>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "testing/gtest/include/gtest/gtest.h"

@implementation FakeChangeProfileCommands {
  raw_ptr<ProfileManagerIOS> _manager;
  std::optional<ChangeProfileParams> _lastChangeProfileParams;
  ChangeProfileContinuation _continuation;
  base::OnceClosure _quitClosure;
  BOOL _shutdownCalled;
}

- (instancetype)init {
  return [self initWithProfileManager:nullptr];
}

- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager {
  if ((self = [super init])) {
    _manager = manager;
    _shutdownCalled = NO;
  }
  return self;
}

- (void)dealloc {
  // Use `CHECK()` instead of `EXPECT_*()` here because `-dealloc` may run
  // during autorelease pool drain after the test body has completed and Google
  // Test's result reporter is inactive.
  CHECK(_shutdownCalled);
  CHECK(!_manager);
  CHECK(!_lastChangeProfileParams.has_value());
  CHECK(_continuation.is_null());
  CHECK(_quitClosure.is_null());
}

- (std::optional<ChangeProfileParams>)lastChangeProfileParams {
  EXPECT_FALSE(_shutdownCalled);
  return _lastChangeProfileParams;
}

- (BOOL)hasContinuation {
  EXPECT_FALSE(_shutdownCalled);
  return !_continuation.is_null();
}

- (void)shutdown {
  _shutdownCalled = YES;
  _manager = nullptr;
  _lastChangeProfileParams = std::nullopt;
  _continuation.Reset();
  _quitClosure.Reset();
}

- (ChangeProfileParams)waitForChangeProfile {
  EXPECT_FALSE(_shutdownCalled);
  if (!_lastChangeProfileParams.has_value()) {
    // `changeProfile:forScene:reason:continuation:` has not been called yet,
    // and may be called asynchronously by a posted task. Spin a nested run loop
    // so that pending tasks are executed. `changeProfile:...` runs
    // `_quitClosure`, which makes `runLoop.Run()` return once the parameters
    // have been recorded.
    base::RunLoop runLoop;
    _quitClosure = runLoop.QuitClosure();
    runLoop.Run();
    _quitClosure.Reset();
  }
  // `ASSERT_*` cannot be used in a method returning a value, so use `EXPECT_*`
  // and return default parameters on failure instead of dereferencing an empty
  // `std::optional`.
  EXPECT_TRUE(_lastChangeProfileParams.has_value());
  return std::exchange(_lastChangeProfileParams, std::nullopt)
      .value_or(ChangeProfileParams());
}

- (void)runContinuationForSceneState:(SceneState*)sceneState {
  EXPECT_FALSE(_shutdownCalled);
  ASSERT_FALSE(_continuation.is_null());
  std::move(_continuation).Run(sceneState, base::DoNothing());
}

#pragma mark - ChangeProfileCommands

- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  EXPECT_FALSE(_shutdownCalled);
  // Ensure we never silently overwrite an unconsumed change.
  EXPECT_FALSE(_lastChangeProfileParams.has_value());
  _lastChangeProfileParams =
      ChangeProfileParams{std::string(profileName), sceneState, reason};
  _continuation = std::move(continuation);
  if (_quitClosure) {
    std::move(_quitClosure).Run();
  }
}

- (void)deleteProfile:(std::string_view)profileName {
  EXPECT_FALSE(_shutdownCalled);
  if (_manager) {
    _manager->MarkProfileForDeletion(profileName);
  }
}

@end
