// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SendTabToSelfImageDetailTextItem is an item that displays an image, a title
// and a detail text. This item uses multi-lines text field.
@interface SendTabToSelfImageDetailTextItem : TableViewItem

// The name of the image to display (required).
@property(nonatomic, copy) NSString* iconImageName;

// The title text to display.
@property(nonatomic, copy) NSString* text;

// The detail text to display.
@property(nonatomic, copy) NSString* detailText;

// The state displaying a check mark accessory.
@property(nonatomic) BOOL selected;

// The cache GUID for the device being displayed.
@property(nonatomic, copy) NSString* cacheGuid;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_
