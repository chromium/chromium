// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
#define IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_

#import <UIKit/UIKit.h>

@class SceneState;

// Enum used to represent failure to change profile.
enum class ChangeProfileFailure {
  kFeatureDisabled,
  kApplicationNotReady,
  kInvalidSceneStateId,
  kInvalidProfileName,
};

// Protocol for the object invoked during the steps of the profile switching.
@protocol ChangeProfileObserving

// Invoked if the request to change profile failed.
- (void)operationFailed:(ChangeProfileFailure)failure;

// Invoked when the application is ready to change the profile. The view
// controller can be used to animate the transition.
- (void)willStartOperation:(UIViewController*)viewController;

// Invoked when the profile has been loaded and the scene is ready for
// further use (i.e. the UI is started). The profile may have not yet
// reached the ProfileInitStage::kFinal stage if there are any blocking
// stage (such as FRE, search engine, ...).
- (void)operationDidComplete:(UIViewController*)viewController
              withSceneState:(SceneState*)sceneState;

@end

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
//
// The observer will be strongly retained until the operation terminates,
// either successfully or with an error. It should not retain any object
// that can be invalidated when a profile is unloaded.
- (void)changeProfile:(NSString*)profileName
             forScene:(NSString*)sceneIdentifier
             observer:(id<ChangeProfileObserving>)observer;

@end

#endif  // IOS_CHROME_APP_CHANGE_PROFILE_COMMANDS_H_
