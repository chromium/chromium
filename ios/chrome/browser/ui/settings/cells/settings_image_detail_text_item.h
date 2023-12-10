// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// SettingsImageDetailTextItem is an item that displays an image, a title and
// a detail text (optional). This item uses multi-lines text field.
@interface SettingsImageDetailTextItem : TableViewItem

// The image to display (required). If this image should be tinted to match the
// text color (e.g. in dark mode), the provided image should have rendering mode
// UIImageRenderingModeAlwaysTemplate.
@property(nonatomic, strong) UIImage* image;

// The image view's alpha.
@property(nonatomic, assign) CGFloat imageViewAlpha;

// The image View's tint color.
@property(nonatomic, strong) UIColor* imageViewTintColor;

// If true, aligns the image with the first line of text.
@property(nonatomic, assign) BOOL alignImageWithFirstLineOfText;

// The title text to display.
@property(nonatomic, copy) NSString* text;

// UIColor for the cell's textLabel. If not set, `kTextPrimaryColor` is used.
@property(nonatomic, strong) UIColor* textColor;

// The attributed text to display.
@property(nonatomic, copy) NSAttributedString* attributedText;

// The detail text to display.
@property(nonatomic, copy) NSString* detailText;

// UIColor for the cell's detailTextLabel. If not set,
// [UIColor colorNamed:kTextSecondaryColor] is used.
@property(nonatomic, strong) UIColor* detailTextColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
