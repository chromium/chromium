// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_WEB_PAGE_URLS_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_WEB_PAGE_URLS_H_

#import <Foundation/Foundation.h>

// Stores the URL and RSS URLs that are used to identify a website.
@interface WebPageURLs : NSObject

// Designated initializer with `URL` and `RSSURLs` (can be empty).
- (instancetype)initWithURL:(NSURL*)URL
                    RSSURLs:(NSArray<NSURL*>*)RSSURLs NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The URL of the web page.
@property(nonatomic, strong, readonly) NSURL* URL;

// The list of RSS links obtained from the web page. If available, they
// are usually listed inside the HEAD html tag.
@property(nonatomic, strong, readonly) NSArray<NSURL*>* RSSURLs;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_WEB_PAGE_URLS_H_
