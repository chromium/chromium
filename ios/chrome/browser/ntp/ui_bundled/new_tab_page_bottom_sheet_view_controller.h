// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class MagicStackCollectionViewController;
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

// Called when the feed sign-in promo changes visibility in the viewport.
- (void)bottomSheetViewController:
            (NewTabPageBottomSheetViewController*)bottomSheetViewController
    didChangeSigninPromoVisibility:(BOOL)visible;

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

// Snapping states for the bottom sheet.
enum class BottomSheetSnappingState {
  kCollapsed,
  kResting,
  kExpanded,
};

// View controller managing the bottom sheet card, gestures, and subviews for
// the NTP Redesign.
@interface NewTabPageBottomSheetViewController
    : UIViewController <UIScrollViewDelegate>

// Delegate for bottom sheet actions.
@property(nonatomic, weak) id<NewTabPageBottomSheetViewControllerDelegate>
    delegate;

// The feed view controller embedded under the "Read" tab.
@property(nonatomic, strong) UIViewController* feedViewController;

// The feed top section view controller containing promos (e.g. Sign-in promo).
@property(nonatomic, strong) UIViewController* feedTopSectionViewController;

// The magic stack view controller.
@property(nonatomic, strong)
    MagicStackCollectionViewController* magicStackViewController;

// Embeds the Most Visited view.
- (void)embedMostVisitedView:(UIView*)mostVisitedView;

// Clears state and delegates.
- (void)invalidate;

// Handles layout and insets when the feed top section promo is closed.
- (void)handleFeedTopSectionClosed;

// Triggers re-layout and inset recalculation when feed content or promos
// update.
- (void)updateFeedLayout;

// Returns the height of the feed top section promo.
- (CGFloat)feedTopSectionHeight;

// Updates whether the omnibox is in the bottom position.
- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox;

// Returns the total height of the header (MVT + Magic Stack + spacing) above
// the feed content.
- (CGFloat)headerHeight;

// Returns the expanded offset of the bottom sheet.
- (CGFloat)expandedOffset;

// Returns the resting offset of the bottom sheet.
- (CGFloat)restingOffset;

// Returns the collapsed offset of the bottom sheet.
- (CGFloat)collapsedOffset;

// Updates the bottom sheet position to match its current snapping state.
- (void)updateBottomSheetPositionAnimated:(BOOL)animated;

// Scrolls the bottom sheet (or feed) back to the top resting position.
- (void)scrollToTopAnimated:(BOOL)animated;

// Returns YES if the bottom sheet is scrolled to the top.
- (BOOL)isScrolledToTop;

// Collapses the bottom sheet back to its resting position.
- (void)collapseToRestingAnimated:(BOOL)animated;

// Updates the layout mode and constraints for the current trait collection.
- (void)updateLayoutModeForCurrentTraitCollection;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_BOTTOM_SHEET_VIEW_CONTROLLER_H_
