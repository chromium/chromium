// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SAFE_BROWSING_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SAFE_BROWSING_HEADER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

// SafeBrowsingHeaderItem displays an image and a title. This item uses
// multi-lines text field.
@interface SafeBrowsingHeaderItem : TableViewHeaderFooterItem

// The image to display (required). If this image should be tinted to match the
// text color (e.g. in dark mode), the provided image should have rendering mode
// UIImageRenderingModeAlwaysTemplate.
@property(nonatomic, strong) UIImage* image;

// The image View's tint color.
@property(nonatomic, strong) UIColor* imageViewTintColor;

// The title text to display.
@property(nonatomic, copy) NSString* text;

// The attributed text to display.
@property(nonatomic, copy) NSAttributedString* attributedText;

@end

// UITableViewHeaderFooterView subclass.
@interface SafeBrowsingHeaderView : UITableViewHeaderFooterView

// Cell image.
@property(nonatomic, strong) UIImage* image;

// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Sets the image view's tint color.
- (void)setImageViewTintColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SAFE_BROWSING_HEADER_ITEM_H_
