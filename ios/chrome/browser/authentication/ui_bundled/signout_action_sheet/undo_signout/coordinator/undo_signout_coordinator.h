// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNOUT_ACTION_SHEET_UNDO_SIGNOUT_COORDINATOR_UNDO_SIGNOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNOUT_ACTION_SHEET_UNDO_SIGNOUT_COORDINATOR_UNDO_SIGNOUT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SceneState;
@class UndoSignoutCoordinator;
@protocol SystemIdentity;

// Delegate protocol for UndoSignoutCoordinator.
@protocol UndoSignoutCoordinatorDelegate <NSObject>

// Called when the undo sign-out flow is completed or cancelled.
- (void)undoSignoutCoordinatorDidFinish:(UndoSignoutCoordinator*)coordinator;

@end

// Signs the user back in with an identity they just signed out of, from the
// sign-out snackbar's "Undo" action. Always operates on the regular Browser of
// its scene, since sign-in must never be performed in incognito.
@interface UndoSignoutCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<UndoSignoutCoordinatorDelegate> delegate;

// Creates a coordinator that signs in with `identity`, presenting any sign-in
// UI on `viewController` and operating on the regular Browser of the scene
// represented by `sceneState`. All arguments must be non-nil, and the scene
// must have a regular Browser.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                     regularBrowserOfScene:(SceneState*)sceneState
                                  identity:(id<SystemIdentity>)identity
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                     regularBrowserOfScene:(SceneState*)sceneState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                            regularBrowser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNOUT_ACTION_SHEET_UNDO_SIGNOUT_COORDINATOR_UNDO_SIGNOUT_COORDINATOR_H_
