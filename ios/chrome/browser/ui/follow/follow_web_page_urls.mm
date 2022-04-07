// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/follow_web_page_urls.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowWebPageURLs

- (instancetype)initWithWebPageURL:(NSURL*)webPageURL
                          RSSLinks:(NSArray<NSURL*>*)RSSLinks {
  self = [super init];
  if (self) {
    _webPageURL = webPageURL;
    _RSSLinks = RSSLinks;
  }
  return self;
}

@end
