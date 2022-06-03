// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COORDINATORS_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_COORDINATORS_CHROME_COORDINATOR_H_

#import <UIKit/UIKit.h>

@class ChromeCoordinator;
class Browser;

typedef NSMutableArray<ChromeCoordinator*> MutableCoordinatorArray;

// A coordinator object that manages view controllers and other coordinators.
// Members of this class should clean up their own UI when they are deallocated.
// TODO(crbug.com/795832): Move to ui/coordinators.
@interface ChromeCoordinator : NSObject

// Creates a coordinator that uses |viewController| and |browser|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Child coordinators created by this object.
@property(strong, nonatomic, readonly)
    MutableCoordinatorArray* childCoordinators;

// The currently 'active' child coordinator, if any. By default this is the last
// coordinator in |childCoordinators|, but subclasses need not adhere to that.
@property(strong, nonatomic, readonly)
    ChromeCoordinator* activeChildCoordinator;

// The view controller this coordinator was initialized with.
@property(weak, nonatomic, readonly) UIViewController* baseViewController;

// Parent coordinator can set this to allow the child coordinator to push their
// view controller to the navigationController instead of presenting it if
// needed. This is usually the same object as |baseViewController|.
@property(weak, nonatomic, readonly)
    UINavigationController* baseNavigationController;

// The coordinator's Browser, if one was assigned.
@property(assign, nonatomic, readonly) Browser* browser;

// The basic lifecycle methods for coordinators are -start and -stop. These
// are blank template methods; child classes are expected to implement them and
// do not need to invoke the superclass methods. Subclasses of ChromeCoordinator
// that expect to be subclassed should not build functionality into these
// methods.
// Starts the user interaction managed by the receiver.
- (void)start;

// Stops the user interaction managed by the receiver. Called on dealloc.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_COORDINATORS_CHROME_COORDINATOR_H_
