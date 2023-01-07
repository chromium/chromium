// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOWED_WEB_SITE_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOWED_WEB_SITE_H_

#import <Foundation/Foundation.h>

// Represents a followed website.
@interface FollowedWebSite : NSObject

// Convenience initializer that initializes all properties.
- (instancetype)initWithTitle:(NSString*)title
                   webPageURL:(NSURL*)webPageURL
                   faviconURL:(NSURL*)faviconURL
                       RSSURL:(NSURL*)RSSURL
                    available:(BOOL)available;

// Title of the website.
@property(nonatomic, copy) NSString* title;

// URL of the website page.
@property(nonatomic, strong) NSURL* webPageURL;

// URL of the website favicon.
@property(nonatomic, strong) NSURL* faviconURL;

// URL of the website rss link.
@property(nonatomic, strong) NSURL* RSSURL;

// YES if the website is available.
@property(nonatomic, assign) BOOL available;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOWED_WEB_SITE_H_
