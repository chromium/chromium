// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"

@class TabGroupIndicatorView;
@class ToolbarButtonFactory;

// View for the primary toolbar. In an adaptive toolbar paradigm, this is the
// toolbar always displayed.
@interface PrimaryToolbarView : UIView<AdaptiveToolbarView>

// Initialize this View with the button `factory`. To finish the initialization
// of the view, a call to `setUp` is needed.
- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// A tappable view overlapping `locationBarContainer` used when the omnibox is
// hidden by the NTP.
@property(nonatomic, strong) UIView* fakeOmniboxTarget;

// StackView containing the leading buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* leadingStackView;
// StackView containing the trailing buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* trailingStackView;

// Button to cancel the edit of the location bar.
@property(nonatomic, strong, readonly) UIButton* cancelButton;

// Constraints to be activated when the location bar is expanded and positioned
// relatively to the cancel button.
@property(nonatomic, strong, readonly)
    NSMutableArray<NSLayoutConstraint*>* expandedConstraints;
// Constraints to be activated when the location bar is contracted with large
// padding between the location bar and the controls.
@property(nonatomic, strong, readonly)
    NSMutableArray<NSLayoutConstraint*>* contractedConstraints;
// Constraints to be activated when the location bar is expanded without cancel
// button.
@property(nonatomic, strong, readonly)
    NSMutableArray<NSLayoutConstraint*>* contractedNoMarginConstraints;

// Constraint for the bottom of the location bar.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarBottomConstraint;

// Whether the top-left and top-right corners of the toolbar are rounded or
// square.
@property(nonatomic, assign) BOOL topCornersRounded;

// Whether the height should match the height of the "fake" toolbar of the NTP.
@property(nonatomic, assign) BOOL matchNTPHeight;

// View that contains tab group information.
@property(nonatomic, weak) TabGroupIndicatorView* tabGroupIndicatorView;

// Sets all the subviews and constraints of the view. The `topSafeAnchor` needs
// to be set before calling this.
- (void)setUp;

// Adds a view overlapping `locationBarContainer` for use when the omnibox is
// hidden by the NTP.
- (void)addFakeOmniboxTarget;

// Removes `fakeOmniboxTarget` from the view hierarchy.
- (void)removeFakeOmniboxTarget;

// Updates the `tabGroupIndicatorView` availability.
- (void)updateTabGroupIndicatorAvailability;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_
