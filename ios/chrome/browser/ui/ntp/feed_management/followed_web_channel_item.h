// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"

@class WebChannel;

// A table view item representing a web channel.
@interface FollowedWebChannelItem : TableViewImageItem

// Web channel associated with this table view item.
@property(nonatomic, strong) WebChannel* webChannel;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_
