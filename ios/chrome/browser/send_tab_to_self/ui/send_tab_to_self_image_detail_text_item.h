// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"

// SendTabToSelfImageDetailTextItem is a subclass of
// SettingsImageDetailTextItem, just adding a field to store the cache GUID.
@interface SendTabToSelfImageDetailTextItem : SettingsImageDetailTextItem

// The cache GUID for the device being displayed.
@property(nonatomic, copy) NSString* cacheGuid;

@end

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_ITEM_H_
