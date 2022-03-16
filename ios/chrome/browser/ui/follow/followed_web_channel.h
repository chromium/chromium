// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOWED_WEB_CHANNEL_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOWED_WEB_CHANNEL_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/ui/follow/follow_block_types.h"

@class CrURL;

// A view model representing a followed web channel.
@interface FollowedWebChannel : NSObject

// Title of the web channel.
@property(nonatomic, copy) NSString* title;

// URL of the web channel.
@property(nonatomic, strong) CrURL* channelURL;

// YES if the web channel is unavailable.
@property(nonatomic, assign) BOOL unavailable;

// Used to request to unfollow this web channel.
@property(nonatomic, copy) FollowRequestBlock unfollowRequestBlock;

// Used to request to refollow this web channel, if it has been unfollowed.
@property(nonatomic, copy) FollowRequestBlock refollowRequestBlock;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOWED_WEB_CHANNEL_H_
