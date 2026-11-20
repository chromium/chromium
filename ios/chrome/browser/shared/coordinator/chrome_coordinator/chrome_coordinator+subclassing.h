// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Additional designated initializers for ChromeCoordinator subclasses that
// require a regular Browser. Only subclasses that call one of these should
// import this header, and only from their .mm file: importing it in a header
// would make these initializers visible to every importer of that header, and
// every ChromeCoordinator subclass with its own designated initializer would
// then fail to compile under -Wobjc-designated-initializers. A presubmit check
// enforces this.
//
// A subclass that calls one of these from its own designated initializer must
// declare the ChromeCoordinator designated initializers it doesn't use as
// NS_UNAVAILABLE, as required by -Wobjc-designated-initializers. It is strongly
// recommended to do so in the subclass's public header (which doesn't need to
// import this header to do so): otherwise, any other .mm file that imports this
// header can call these initializers on the subclass, skipping the subclass's
// own designated initializer.
@interface ChromeCoordinator ()

// Creates a coordinator that operates on the regular Browser of the scene
// represented by `sceneState`, regardless of which Browser its parent is using
// (see `GetRegularBrowserForScene()`). Use this for features that must never
// run in an incognito context (such as sign-in), so that parents can't start
// them with the wrong Browser.
//
// No Browser is passed in, so initializers in the chain can't hold a Browser
// that differs from `self.browser`. Setup that needs the Browser belongs in
// `-start`; an initializer that truly needs it should call
// `GetRegularBrowserForScene(sceneState)` rather than use `self.browser`.
//
// `viewController` is used as provided, and may belong to the incognito UI.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                     regularBrowserOfScene:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

// Creates a coordinator that uses `viewController` and `browser`, where
// `browser` must be a regular Browser; any other Browser is a CHECK failure.
// Use this instead of an ad-hoc CHECK in a subclass initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                            regularBrowser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_CHROME_COORDINATOR_SUBCLASSING_H_
