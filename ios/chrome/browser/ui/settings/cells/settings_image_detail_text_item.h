// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SettingsImageDetailTextItem is an item that displays an image, a title and
// a detail text (optional). This item uses multi-lines text field.
@interface SettingsImageDetailTextItem : TableViewItem

// The image to display (required). If this image should be tinted to match the
// text color (e.g. in dark mode), the provided image should have rendering mode
// UIImageRenderingModeAlwaysTemplate.
@property(nonatomic, strong) UIImage* image;

// Whether the image should be tinted as an icon or not (if it is already
// colored). The tint color will match the text color.
@property(nonatomic, assign) BOOL imageShouldBeTinted;

// The title text to display.
@property(nonatomic, copy) NSString* text;

// The detail text to display.
@property(nonatomic, copy) NSString* detailText;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
