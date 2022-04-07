// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_WEB_PAGE_URLS_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_WEB_PAGE_URLS_H_

#import <Foundation/Foundation.h>

// The URLs of a webpage that are used to identify to which web channel it
// belongs. Note that a given webpage may belong to several different web
// channels.
@interface FollowWebPageURLs : NSObject

// Designated initializer with |webPageURL| and |RSSLinks|.
// |RSSlinks| can be empty.
- (instancetype)initWithWebPageURL:(NSURL*)webPageURL
                          RSSLinks:(NSArray<NSURL*>*)RSSLinks
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The URL of the webpage.
@property(nonatomic, strong, readonly) NSURL* webPageURL;
// The list of RSS links obtained from the webpage. If available, they are
// usually listed inside the HEAD html tag.
@property(nonatomic, strong, readonly) NSArray<NSURL*>* RSSLinks;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_WEB_PAGE_URLS_H_
