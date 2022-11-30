// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/empty_nsurlcache.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation EmptyNSURLCache

+ (instancetype)emptyNSURLCache {
  return [[EmptyNSURLCache alloc] init];
}

- (NSCachedURLResponse*)cachedResponseForRequest:(NSURLRequest*)request {
  return nil;
}

@end
