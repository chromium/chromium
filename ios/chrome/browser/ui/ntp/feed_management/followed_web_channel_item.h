// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

@class FollowedWebChannel;

// A table view item representing a web channel.
@interface FollowedWebChannelItem : TableViewURLItem

// Web channel associated with this table view item.
@property(nonatomic, strong) FollowedWebChannel* followedWebChannel;

@end

// A table view cell representing a web channel.
@interface FollowedWebChannelCell : TableViewURLCell

// Web channel associated with this cell.
@property(nonatomic, weak) FollowedWebChannel* followedWebChannel;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNEL_ITEM_H_
