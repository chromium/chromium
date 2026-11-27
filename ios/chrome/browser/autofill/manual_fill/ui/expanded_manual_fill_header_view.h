// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_EXPANDED_MANUAL_FILL_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_EXPANDED_MANUAL_FILL_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class ExpandedManualFillHeaderView;

namespace manual_fill {
enum class ManualFillDataType;
}  // namespace manual_fill

// Delegate receiving user interaction events from
// `ExpandedManualFillHeaderView`.
@protocol ExpandedManualFillHeaderViewDelegate <NSObject>

// Invoked when the user taps the close button.
- (void)headerView:(ExpandedManualFillHeaderView*)headerView
    didPressCloseButton:(UIButton*)closeButton;

// Invoked when the user selects `dataType` in the category control.
- (void)headerView:(ExpandedManualFillHeaderView*)headerView
    didSelectDataType:(manual_fill::ManualFillDataType)dataType;

@end

// Header view presented at the top of `ExpandedManualFillViewController`,
// adapting between narrow (portrait / popover) and wide (compact-height
// landscape) layouts.
@interface ExpandedManualFillHeaderView : UIView

// Delegate notified of close button taps and category selection changes.
@property(nonatomic, weak) id<ExpandedManualFillHeaderViewDelegate> delegate;

// Leading and trailing horizontal inset aligning header controls with the
// child table view's cells.
@property(nonatomic, assign) CGFloat horizontalInset;

// Whether controls are laid out in a single row (wide layout, used in compact
// height) instead of stacked rows (narrow layout). Defaults to NO.
@property(nonatomic, assign) BOOL wideLayout;

// Element that should receive accessibility focus when the expanded view
// appears.
@property(nonatomic, strong, readonly) UIView* preferredAccessibilityFocusView;

// Control allowing switching between the different data types.
@property(nonatomic, strong, readonly) UISegmentedControl* segmentedControl;

// Designated initializer selecting `initialDataType` in the category control.
- (instancetype)initWithDataType:
    (manual_fill::ManualFillDataType)initialDataType NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_EXPANDED_MANUAL_FILL_HEADER_VIEW_H_
