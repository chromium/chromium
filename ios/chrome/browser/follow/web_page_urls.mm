// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/web_page_urls.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebPageURLs

- (instancetype)initWithURL:(NSURL*)URL RSSURLs:(NSArray<NSURL*>*)RSSURLs {
  if ((self = [super init])) {
    _URL = URL;
    _RSSURLs = [RSSURLs copy];
  }
  return self;
}

@end
