// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/empty_nsurlcache.h"

@implementation EmptyNSURLCache

+ (instancetype)emptyNSURLCache {
  return [[EmptyNSURLCache alloc] init];
}

- (NSCachedURLResponse*)cachedResponseForRequest:(NSURLRequest*)request {
  return nil;
}

@end
