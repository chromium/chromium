// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/crurl.h"

#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CrURL {
  // In an attempt to reduce the number of conversions to NSURL, we strive
  // to keep URLs represented as GURL for as long as possible. It should only
  // be converted to NSURL when needed by UIKit or Foundation as some URLs
  // are not able to be perfectly represented as NSURL, or the conversion is
  // not always accurate. This is similar to what is done with NSString.
  GURL url_;
}

- (instancetype)initWithGURL:(const GURL&)url {
  if ((self = [super init])) {
    url_ = url;
  }
  return self;
}

- (instancetype)initWithNSURL:(NSURL*)url {
  return [self initWithGURL:net::GURLWithNSURL(url)];
}

- (const GURL&)gurl {
  return url_;
}

- (NSURL*)nsurl {
  return net::NSURLWithGURL(url_);
}

@end
