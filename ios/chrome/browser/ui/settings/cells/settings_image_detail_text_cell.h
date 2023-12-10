// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_CELL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Cell representation for SettingsImageDetailTextItem.
//  +--------------------------------------------------+
//  |  +-------+                                       |
//  |  | image |   Multiline title                     |
//  |  |       |   Optional multiline detail text      |
//  |  +-------+                                       |
//  +--------------------------------------------------+
@interface SettingsImageDetailTextCell : TableViewCell

// Cell image.
@property(nonatomic, strong) UIImage* image;

// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Sets the image view's alpha.
- (void)setImageViewAlpha:(CGFloat)alpha;

// Sets the image view's tint color.
- (void)setImageViewTintColor:(UIColor*)color;

// If alignImageWithFirstBaseline is YES, activates the constraints to align the
// image with the first line of text.
- (void)alignImageWithFirstLineOfText:(BOOL)alignImageWithFirstBaseline;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_CELL_H_
