// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// SettingsImageDetailTextItem is an item that displays an image, a title and
// a detail text (optional). This item uses multi-lines text field. It also
// contains acommand id.
// This class is intended to be used with Google services settings view.
@interface SettingsImageDetailTextItem : CollectionViewItem

// The image to display.
@property(nonatomic, strong) UIImage* image;

// The title text to display.
@property(nonatomic, copy) NSString* text;

// The detail text to display.
@property(nonatomic, copy) NSString* detailText;

// Command to trigger when the switch is toggled. The default value is 0.
@property(nonatomic, assign) NSInteger commandID;

@end

// Cell representation for SettingsImageDetailTextItem.
//  +--------------------------------------------------+
//  |  +-------+                                       |
//  |  | image |   Multiline title                     |
//  |  |       |   Optional multiline detail text      |
//  |  +-------+                                       |
//  +--------------------------------------------------+
@interface SettingsImageDetailTextCell : MDCCollectionViewCell

// Cell image.
@property(nonatomic, readonly, strong) UIImageView* imageView;

// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_IMAGE_DETAIL_TEXT_ITEM_H_
