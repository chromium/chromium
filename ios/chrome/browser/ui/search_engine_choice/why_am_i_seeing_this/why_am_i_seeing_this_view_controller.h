// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class WhyAmISeeingThisViewController;

// Delegate protocol for WhyAmISeeingThisViewController.
@protocol WhyAmISeeingThisDelegate <NSObject>

// Called when the user taps the dismiss button.
- (void)learnMoreDone:(WhyAmISeeingThisViewController*)viewController;

@end

// A base view controller for showing an informational screen.
@interface WhyAmISeeingThisViewController : LegacyChromeTableViewController

// View controller delegate.
@property(nonatomic, weak) id<WhyAmISeeingThisDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_VIEW_CONTROLLER_H_
