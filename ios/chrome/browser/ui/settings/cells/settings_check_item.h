// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Possible warning states of a SettingsCheckItem.
enum class WarningState {
  kSafe,     // Everything is good, no warning to show (green icon).
  kWarning,  // There is a warning, but it's not high priority (yellow icon).
  kSevereWarning,  // There is a high priority warning (red icon).
};

// SettingsCheckItem is a model class that uses SettingsCheckCell.
@interface SettingsCheckItem : TableViewItem

// The text to display.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The image to display on the leading side of `text` (optional). If this image
// should be tinted to match the text color (e.g. in dark mode), the provided
// image should have rendering mode UIImageRenderingModeAlwaysTemplate.
@property(nonatomic, strong) UIImage* leadingIcon;

// Tint color for `leadingImage`.
@property(nonatomic, copy) UIColor* leadingIconTintColor;

// The background color of the icon.
@property(nonatomic, strong) UIColor* leadingIconBackgroundColor;

// The corner radius of the UIImage view.
@property(nonatomic, assign) CGFloat leadingIconCornerRadius;

// The image to display on the trailing side of `text` (required). If this image
// should be tinted to match the text color (e.g. in dark mode), the provided
// image should have rendering mode UIImageRenderingModeAlwaysTemplate. Don't
// set image with `isIndicatorHidden` equal to false as image won't be shown
// in that case.
@property(nonatomic, strong) UIImage* trailingImage;

// Tint color for `trailingImage`.
@property(nonatomic, copy) UIColor* trailingImageTintColor;

// Controls visibility of `activityIndicator`, if set false `trailingImage` or
// `infoButton` will be hidden and `activityIndicator` will be shown. This
// property has the highest priority.
@property(nonatomic, assign, getter=isIndicatorHidden) BOOL indicatorHidden;

// Controls visibility of `infoButton`. This property has no effect in case
// `trailingImage` is provided or `indicatorHidden` is false.
@property(nonatomic, assign, getter=isInfoButtonHidden) BOOL infoButtonHidden;

// Disabled cell are automatically drawn with dimmed text and without
// `trailingImage` or `activityIndicator`.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// The WarningState of the item.
@property(nonatomic, assign) WarningState warningState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_ITEM_H_
