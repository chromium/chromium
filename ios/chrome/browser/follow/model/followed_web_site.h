// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/follow/model/followed_web_site_state.h"

// Represents a followed website.
@interface FollowedWebSite : NSObject

// Title of the website.
@property(nonatomic, copy) NSString* title;

// URL of the website page.
@property(nonatomic, strong) NSURL* webPageURL;

// URL of the website favicon.
@property(nonatomic, strong) NSURL* faviconURL;

// URL of the website rss link.
@property(nonatomic, strong) NSURL* RSSURL;

// State of the website.
@property(nonatomic, assign) FollowedWebSiteState state;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_H_
