// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"

class Browser;

// Mediator which mediates between openIn views and openIn tab helpers.
@interface OpenInMediator : NSObject <OpenInTabHelperDelegate>

// Creates a mediator that uses a `viewController` and a `browser`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disables all registered openInControllers.
- (void)disableAll;

// Dismisses all the activity controller window.
- (void)dismissAll;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_
