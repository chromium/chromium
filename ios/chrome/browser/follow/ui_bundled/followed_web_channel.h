// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FOLLOWED_WEB_CHANNEL_H_
#define IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FOLLOWED_WEB_CHANNEL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/follow/model/followed_web_site_state.h"

@class CrURL;

// A view model representing a followed web channel.
// TODO(crbug.com/40232524): remove this class when code has been converted
// to use FollowedWebSite instead.
@interface FollowedWebChannel : NSObject

// Title of the web channel.
@property(nonatomic, copy) NSString* title;

// URL of the web channel web page.
@property(nonatomic, strong) CrURL* webPageURL;

// URL of the web channel rss.
@property(nonatomic, strong) CrURL* rssURL;

// URL of the favicon.
@property(nonatomic, strong) CrURL* faviconURL;

// State of the website.
@property(nonatomic, assign) FollowedWebSiteState state;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FOLLOWED_WEB_CHANNEL_H_
