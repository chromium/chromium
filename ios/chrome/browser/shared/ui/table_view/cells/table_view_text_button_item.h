// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewTextButtonItem contains the model for
// TableViewTextButtonCell.
@interface TableViewTextButtonItem : TableViewItem

// Text being displayed above the button.
@property(nonatomic, readwrite, strong) NSString* text;

// Text being displayed above the button alignment.
@property(nonatomic, readwrite, assign) NSTextAlignment textAlignment;

// Text for cell button.
@property(nonatomic, readwrite, strong) NSString* buttonText;

// Button text color.
@property(nonatomic, strong) UIColor* buttonTextColor;

// Button background color. Default is custom blue color.
@property(nonatomic, strong) UIColor* buttonBackgroundColor;

// If yes, adds a 50% alpha to the background in disabled state.
// Otherwise, colors in disabled state are the same as in enabled
// state and it is the responsibility of the owner to update color
// before calling `configureCell:withStyler:` (default YES).
@property(nonatomic, assign) BOOL dimBackgroundWhenDisabled;

// Whether the button text will be bold or not. Default is YES.
@property(nonatomic, assign) BOOL boldButtonText;

// Accessibility identifier that will assigned to the button.
@property(nonatomic, strong) NSString* buttonAccessibilityIdentifier;

// Whether the Item's button should be enabled or not. Button is enabled by
// default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// If YES the item's button width will expand to match the cell's. If NO the
// button will maintain its intrinsic size based on its title. NO by default.
@property(nonatomic, assign) BOOL disableButtonIntrinsicWidth;

// Whether the Item's button should display an activity indicator. Default is
// NO.
@property(nonatomic, assign) BOOL showsActivityIndicator;

// Activity Indicator color. If nil, the activity indicator will be of a solid
// white color.
@property(nonatomic, strong) UIColor* activityIndicatorColor;

// Whether the Item's button should display a checkmark image indicating action
// has been completed. Default is NO.
@property(nonatomic, assign) BOOL showsCheckmark;

// Checkmark image color. If nil, defaults to kBlue700Color.
@property(nonatomic, strong) UIColor* checkmarkColor;

// Accessibility label that will assigned to the button.
@property(nonatomic, strong) NSString* buttonAccessibilityLabel;

@end

// TableViewTextButtonCell contains a textLabel and a UIbutton
// laid out vertically and centered.
@interface TableViewTextButtonCell : TableViewCell

// Cell text information.
@property(nonatomic, strong) UILabel* textLabel;

// Action button. Note: Set action method in the TableView datasource method.
@property(nonatomic, strong) UIButton* button;

// Enables spacing between items. If there's only one item (e.g. just a button),
// disable the spacing or an extra top padding will be added.
- (void)enableItemSpacing:(BOOL)enable;

// If `disabled` is YES the button's width will expand to match the cell's
// container. If NO, the button will maintain its intrinsic size based on its
// title.
- (void)disableButtonIntrinsicWidth:(BOOL)disable;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_
