// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/follow_site_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowSiteInfo

- (instancetype)initWithPageURL:(NSURL*)siteURL
                       rssLinks:(NSArray<NSURL*>*)rssLinks {
  self = [super init];
  if (self) {
    _siteURL = siteURL;
    _rssLinks = rssLinks;
  }
  return self;
}

@end
