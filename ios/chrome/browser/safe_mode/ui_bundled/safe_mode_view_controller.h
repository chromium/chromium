// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A protocol required by delegates of the SafeModeViewController.
@protocol SafeModeViewControllerDelegate
@required
// Tell delegate to attempt to start the browser.
- (void)startBrowserFromSafeMode;
@end

@interface SafeModeViewController : UIViewController

- (id)initWithDelegate:(id<SafeModeViewControllerDelegate>)delegate;

// Returns `YES` when the safe mode UI has information to show.
+ (BOOL)hasSuggestions;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_MODE_UI_BUNDLED_SAFE_MODE_VIEW_CONTROLLER_H_
