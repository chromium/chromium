// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TEST_FAKE_CHANGE_PROFILE_COMMANDS_H_
#define IOS_CHROME_APP_TEST_FAKE_CHANGE_PROFILE_COMMANDS_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "ios/chrome/app/change_profile_commands.h"

class ProfileManagerIOS;
@class SceneState;

// Parameters passed to a
// `-[ChangeProfileCommands changeProfile:forScene:reason:continuation:]` call.
struct ChangeProfileParams {
  std::string profile_name;
  __weak SceneState* scene_state = nil;
  ChangeProfileReason reason;

  friend bool operator==(const ChangeProfileParams& lhs,
                         const ChangeProfileParams& rhs) = default;
};

// Fake implementation of `ChangeProfileCommands` for unit tests.
@interface FakeChangeProfileCommands : NSObject <ChangeProfileCommands>

// Parameters of the most recent `changeProfile:forScene:reason:continuation:`
// call declared in `ChangeProfileCommands`, or `std::nullopt` if not called (or
// after `waitForChangeProfile` consumed it). Allows tests to assert on the
// arguments passed to `changeProfile:forScene:reason:continuation:`.
@property(nonatomic, readonly) std::optional<ChangeProfileParams>
    lastChangeProfileParams;

// Whether a non-null `ChangeProfileContinuation` is currently stored from the
// most recent `changeProfile:forScene:reason:continuation:` call declared in
// `ChangeProfileCommands`. Allows tests to check if a continuation was
// provided.
@property(nonatomic, readonly) BOOL hasContinuation;

// Initializes the fake handler with a null profile manager.
- (instancetype)init;

// Initializes the fake handler. If `manager` is non-null, `deleteProfile:` also
// marks the profile for deletion in `manager`.
- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager
    NS_DESIGNATED_INITIALIZER;

// Resets all state and clears held pointers before deallocation. Must be called
// by tests before this object is deallocated.
- (void)shutdown;

// Waits until `changeProfile:forScene:reason:continuation:` is called (declared
// in `ChangeProfileCommands`), then consumes and returns its
// `ChangeProfileParams` (resetting `lastChangeProfileParams` to
// `std::nullopt`).
- (ChangeProfileParams)waitForChangeProfile;

// Invokes the stored `ChangeProfileContinuation` received via
// `changeProfile:forScene:reason:continuation:` for `sceneState` with a no-op
// completion closure. `hasContinuation` must be `YES`.
- (void)runContinuationForSceneState:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_APP_TEST_FAKE_CHANGE_PROFILE_COMMANDS_H_
