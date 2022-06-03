// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_

#import <UIKit/UIKit.h>


@class AlertCoordinator;
class Browser;
@class BrowserViewControllerHelper;
@class KeyCommandsProvider;

// Creates helper objects needed by BrowserViewController.
@interface BrowserViewControllerDependencyFactory : NSObject

// Creates a new factory backed by |browser|. This must be the same browser
// provided to BrowserViewController (and like BVC, this is a weak reference).
- (instancetype)initWithBrowser:(Browser*)browser;

- (BrowserViewControllerHelper*)newBrowserViewControllerHelper;

// Returns a new keyboard commands coordinator to handle keyboard commands.
- (KeyCommandsProvider*)newKeyCommandsProvider;

- (AlertCoordinator*)alertCoordinatorWithTitle:(NSString*)title
                                       message:(NSString*)message
                                viewController:
                                    (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
