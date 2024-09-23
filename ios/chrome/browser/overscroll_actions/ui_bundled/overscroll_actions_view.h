// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_VIEW_H_
#define IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/time/time.h"

// Describes the current Index of an action in the OverScrollActionsView.
enum class OverscrollAction {
  NONE,       // No action
  NEW_TAB,    // New tab action
  REFRESH,    // Refresh action
  CLOSE_TAB,  // Close tab action
};

// Describes the style of the overscroll action UI.
enum class OverscrollStyle {
  NTP_NON_INCOGNITO,           // UI to fit the NTP in non incognito.
  NTP_INCOGNITO,               // UI to fit the NTP in incognito.
  REGULAR_PAGE_NON_INCOGNITO,  // UI to fit regular pages in non incognito.
  REGULAR_PAGE_INCOGNITO       // UI to fit regular pages in incognito.
};

// Minimum delay for the view to perform the transition to the ready state.
constexpr base::TimeDelta kMinimumPullDurationToTransitionToReady =
    base::Milliseconds(250);

// The brightness of the actions view background color for non incognito mode.
extern const CGFloat kActionViewBackgroundColorBrightnessNonIncognito;

// The brightness of the actions view background color for incognito mode.
extern const CGFloat kActionViewBackgroundColorBrightnessIncognito;

@class OverscrollActionsView;

@protocol OverscrollActionsViewDelegate

// Called when the user explicitly taps on one of the items to trigger its
// action.
- (void)overscrollActionsViewDidTapTriggerAction:
    (OverscrollActionsView*)overscrollActionsView;

// Called after the selectedAction property changed and the animations have
// been triggered.
- (void)overscrollActionsView:(OverscrollActionsView*)view
      selectedActionDidChange:(OverscrollAction)newAction;

@end

// This view displays the actions of the OverscrollActionsController.
// How actions are displayed depends on the vertical and horizontal offsets.
@interface OverscrollActionsView : UIView

// The currently selected action.
@property(nonatomic, assign, readonly) OverscrollAction selectedAction;
// The view displayed has the background of the overscroll actions view.
@property(nonatomic, strong, readonly) UIView* backgroundView;
// Whether cropping is set on the selection circle (true by default).
@property(nonatomic, assign) BOOL selectionCroppingEnabled;
// The current style of the overscroll actions UI.
@property(nonatomic, assign) OverscrollStyle style;

@property(nonatomic, weak) id<OverscrollActionsViewDelegate> delegate;

// Add a snapshot view on top of the background image view. The previous
// snapshot view if any will be removed.
- (void)addSnapshotView:(UIView*)view;

// Called to indicate that a new pull gesture has started.
- (void)pullStarted;

// Vertical offset [0, yOffset].
// This offset is set to the top inset of the scrollView managed by the
// OverscrollActionsController.
- (void)updateWithVerticalOffset:(CGFloat)offset;

// Horizontal offset [-1,1].
// This offset is set by the OverscrollActionsController and is used to display
// the action selection circle.
- (void)updateWithHorizontalOffset:(CGFloat)offset;

// This starts an "ink response" like animation typically triggered when an
// action has been selected.
- (void)displayActionAnimation;

@end

#endif  // IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_VIEW_H_
