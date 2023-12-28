// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_CRURL_H_
#define IOS_CHROME_BROWSER_NET_MODEL_CRURL_H_

#import <Foundation/Foundation.h>

#ifdef __cplusplus
#include "url/gurl.h"
#endif

// A pure ObjectiveC wrapper to help keep APIs clean of C++ uses of
// GURL, which will help them be more usable from Swift.

@interface CrURL : NSObject

#ifdef __cplusplus
- (instancetype)initWithGURL:(const GURL&)url;
#endif

- (instancetype)initWithNSURL:(NSURL*)url;

#ifdef __cplusplus
@property(nonatomic, readonly) const GURL& gurl;
#endif

@property(nonatomic, readonly) NSURL* nsurl;

@end

#endif  // IOS_CHROME_BROWSER_NET_MODEL_CRURL_H_
