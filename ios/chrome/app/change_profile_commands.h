// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
#define IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_

#import <string_view>

#import "base/functional/callback_forward.h"
#import "ios/chrome/app/change_profile_continuation.h"

@class SceneState;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSChangeProfileReason)
enum class ChangeProfileReason {
  kSwitchAccounts = 0,
  kManagedAccountSignIn = 1,
  kManagedAccountSignOut = 2,
  kAuthenticationError = 3,
  kProfileDeleted = 4,
  kHandlePushNotification = 5,
  kSwitchAccountsFromWidget = 6,
  kSwitchAccountsFromShareExtension = 7,
  kMaxValue = kSwitchAccountsFromShareExtension
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IOSChangeProfileReason)

// App-level commands related to switching profiles.
@protocol ChangeProfileCommands

// Changes the profile used by the scene identified by `sceneState` and invokes
// `continuation` when the profile is fully loaded.
//
// The profile named `profileName` must be registered already, but it does not
// need to be initialized or loaded. This method will take care of initializing
// it, loading it, and then connecting the scene with the profile.
//
// The continuation will be called asynchronously, when the profile has
// been switched for the SceneState.
- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation;

// Deletes the profile named `profileName` (the data may be deleted at
// a later time and the profile itself will be unloaded asynchronously).
// All the scenes currently connected to this profile will switch to the
// personal profile (with an animation).
- (void)deleteProfile:(std::string_view)profileName;

@end

#endif  // IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
