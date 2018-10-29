// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view.h"

@class ToolbarButtonFactory;

// View for the primary toolbar. In an adaptive toolbar paradigm, this is the
// toolbar always displayed.
@interface PrimaryToolbarView : UIView<AdaptiveToolbarView>

// Initialize this View with the button |factory|. To finish the initialization
// of the view, a call to |setUp| is needed.
- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The location bar view, containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;

// Container for the location bar.
@property(nonatomic, strong, readonly) UIView* locationBarContainer;

// A tappable view overlapping |locationBarContainer| used when the omnibox is
// hidden by the NTP.
@property(nonatomic, strong) UIView* fakeOmniboxTarget;

// The height of the container for the location bar.
@property(nonatomic, strong, readonly) NSLayoutConstraint* locationBarHeight;

// StackView containing the leading buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* leadingStackView;
// StackView containing the trailing buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* trailingStackView;

// Button to cancel the edit of the location bar.
@property(nonatomic, strong, readonly) UIButton* cancelButton;

// Button taking the full size of the toolbar. Expands the toolbar when  tapped.
@property(nonatomic, strong, readonly) UIButton* collapsedToolbarButton;

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
// Constraint for extra padding on the bottom of the location bar. This padding
// is considered as "extra" as it is added to the one defined in
// |locationBarBottomConstraint|. See comment for -[PrimaryToolbarViewController
// verticalMarginForLocationBarForFullscreenProgress:] for more explanations.
@property(nonatomic, strong) NSLayoutConstraint* locationBarExtraBottomPadding;

// Sets all the subviews and constraints of the view. The |topSafeAnchor| needs
// to be set before calling this.
- (void)setUp;

// Adds a view overlapping |locationBarContainer| for use when the omnibox is
// hidden by the NTP.
- (void)addFakeOmniboxTarget;

// Removes |fakeOmniboxTarget| from the view hierarchy.
- (void)removeFakeOmniboxTarget;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_H_
