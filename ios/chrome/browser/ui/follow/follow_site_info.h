// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_SITE_INFO_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_SITE_INFO_H_

#import <Foundation/Foundation.h>

// The information of a website, which is used to check and update its following
// status.
@interface FollowSiteInfo : NSObject

// Initiates a FollowSiteInfo with |siteURL| and |RSSLinks|.
// |RSSlinks| can be empty.
- (instancetype)initWithPageURL:(NSURL*)siteURL
                       RSSLinks:(NSArray<NSURL*>*)RSSLinks
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The URL of the site.
@property(nonatomic, strong, readonly) NSURL* siteURL;
// The list of RSS links.
@property(nonatomic, strong, readonly) NSArray<NSURL*>* RSSLinks;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_SITE_INFO_H_
