// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_H_

#import <UIKit/UIKit.h>

@class ChromeCoordinator;
@class SceneState;
class Browser;
class ProfileIOS;

typedef NSMutableArray<ChromeCoordinator*> MutableCoordinatorArray;

// Base class for coordinators that manage a UI feature scoped to a Browser.
// Subclasses must set up their UI and dependencies in `-start` and tear them
// down in `-stop` (never in `-dealloc`).
// See //ios/architecture/coordinators.md for details.
NS_SWIFT_UI_ACTOR
@interface ChromeCoordinator : NSObject

// Creates a coordinator that uses `viewController` and `browser`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Legacy array of child coordinators. Prefer storing child coordinators in
// explicit, typed instance variables or properties instead.
@property(strong, nonatomic, readonly)
    MutableCoordinatorArray* childCoordinators;

// Legacy convenience property returning `childCoordinators.lastObject` by
// default. Avoid using in new code.
@property(strong, nonatomic, readonly)
    ChromeCoordinator* activeChildCoordinator;

// The view controller this coordinator was initialized with.
@property(weak, nonatomic, readonly) UIViewController* baseViewController;

// Parent coordinator can set this to allow the child coordinator to push their
// view controller to the navigationController instead of presenting it if
// needed. This is usually the same object as `baseViewController`.
@property(weak, nonatomic, readonly)
    UINavigationController* baseNavigationController;

// The coordinator's Browser. Must not be accessed after `-stop`.
@property(assign, nonatomic, readonly) Browser* browser;

// Convenience getter for `self.browser->GetProfile()`. Must not be accessed
// after `-stop`.
@property(assign, nonatomic, readonly) ProfileIOS* profile;

// Convenience getter for `self.browser->GetSceneState()`.
@property(weak, nonatomic, readonly) SceneState* sceneState;

// Convenience getter for `self.profile->IsOffTheRecord()`.
- (BOOL)isOffTheRecord;

// The basic lifecycle methods for coordinators are -start and -stop. These
// are blank template methods; child classes are expected to implement them and
// do not need to invoke the superclass methods. Subclasses of ChromeCoordinator
// that expect to be subclassed should not build functionality into these
// methods.
// Starts the user interaction managed by the receiver.
- (void)start;

// Stops the user interaction managed by the receiver.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_H_
