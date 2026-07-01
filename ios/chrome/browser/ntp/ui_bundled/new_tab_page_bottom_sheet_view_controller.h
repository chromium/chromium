// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class NewTabPageBottomSheetViewController;

// Delegate for events in the bottom sheet view controller.
@protocol NewTabPageBottomSheetViewControllerDelegate <NSObject>

// Called when the fake location bar in the bottom sheet is tapped.
- (void)bottomSheetViewControllerDidTapFakeLocationBar:
    (NewTabPageBottomSheetViewController*)bottomSheetViewController;

@end

// View controller managing the bottom sheet card, gestures, and subviews for
// the NTP Redesign.
@interface NewTabPageBottomSheetViewController : UIViewController

// Delegate for bottom sheet actions.
@property(nonatomic, weak) id<NewTabPageBottomSheetViewControllerDelegate>
    delegate;

// The feed view controller embedded under the "Read" tab.
@property(nonatomic, strong) UIViewController* feedViewController;

// Clears state and delegates.
- (void)invalidate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
