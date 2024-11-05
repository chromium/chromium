// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
#define IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_

#import <Foundation/Foundation.h>

// Callback invoked when the profile change request is complete.
using ChangeProfileCompletion = void (^)(bool success);

// App-level commands related to switching profiles.
@protocol ChangeProfileCommands

// Change the profile used by the scene with `sceneIdentifier` and invoke
// `completion` when the profile is fully initialised (or as soon as the
// operation fails in case of failure).
//
// This can be called even if the profile named `profileName` has not yet
// been created, the method will take care of creating it, loading it and
// then connecting the scene with the profile.
//
// The method may fail if the feature kSeparateProfilesForManagedAccounts
// is disabled or not available (on iOS < 17), if creating the profile is
// impossible or fails, or if no scene named `sceneIdentifier` exists.
- (void)changeProfile:(NSString*)profileName
             forScene:(NSString*)sceneIdentifier
           completion:(ChangeProfileCompletion)completion;

@end

#endif  // IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
