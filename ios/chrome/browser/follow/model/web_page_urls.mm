// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/web_page_urls.h"

@implementation WebPageURLs

- (instancetype)initWithURL:(NSURL*)URL RSSURLs:(NSArray<NSURL*>*)RSSURLs {
  if ((self = [super init])) {
    _URL = URL;
    _RSSURLs = [RSSURLs copy];
  }
  return self;
}

@end
