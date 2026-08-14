// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class NewTabPageBottomSheetViewController;

// Delegate for events in the bottom sheet view controller.
@protocol NewTabPageBottomSheetViewControllerDelegate <NSObject>

// Called when the bottom sheet top offset is updated.
- (void)bottomSheetViewController:
            (NewTabPageBottomSheetViewController*)bottomSheetViewController
               didUpdateTopOffset:(CGFloat)topOffset;

// Called when the user performs the VoiceOver escape gesture on the bottom
// sheet.
- (void)bottomSheetViewControllerDidEscape:
    (NewTabPageBottomSheetViewController*)bottomSheetViewController;

// Returns the preferred resting offset for the bottom sheet.
- (CGFloat)restingOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController;

// Returns the preferred collapsed offset for the bottom sheet.
- (CGFloat)collapsedOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController;

// Returns the preferred expanded offset for the bottom sheet (docked below
// the toolbar or safe area).
- (CGFloat)expandedOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController;

@end

// View controller managing the bottom sheet card, gestures, and subviews for
// the NTP Redesign.
@interface NewTabPageBottomSheetViewController
    : UIViewController <UIScrollViewDelegate>

// Delegate for bottom sheet actions.
@property(nonatomic, weak) id<NewTabPageBottomSheetViewControllerDelegate>
    delegate;

// The feed view controller embedded under the "Read" tab.
@property(nonatomic, strong) UIViewController* feedViewController;

// The magic stack view controller.
@property(nonatomic, strong) UIViewController* magicStackViewController;

// Embeds the Most Visited view.
- (void)embedMostVisitedView:(UIView*)mostVisitedView;

// Clears state and delegates.
- (void)invalidate;

// Updates whether the omnibox is in the bottom position.
- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox;

// Returns the expanded offset of the bottom sheet.
- (CGFloat)expandedOffset;

// Returns the resting offset of the bottom sheet.
- (CGFloat)restingOffset;

// Returns the collapsed offset of the bottom sheet.
- (CGFloat)collapsedOffset;

// Updates the bottom sheet position to match its current snapping state.
- (void)updateBottomSheetPositionAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
