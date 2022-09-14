// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_EMPTY_NSURLCACHE_H_
#define IOS_NET_EMPTY_NSURLCACHE_H_

#import <Foundation/Foundation.h>

// Dummy NSURLCache implementation that does not cache anything.
@interface EmptyNSURLCache : NSURLCache
+ (instancetype)emptyNSURLCache;
@end

#endif  // IOS_NET_EMPTY_NSURLCACHE_H_
