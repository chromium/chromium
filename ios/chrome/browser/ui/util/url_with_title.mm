// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/url_with_title.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface URLWithTitle () {
  // URL to be shared.
  GURL _URL;
}
@end

@implementation URLWithTitle

- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title {
  DCHECK(URL.is_valid());
  DCHECK(title);
  self = [super init];
  if (self) {
    _URL = URL;
    _title = [title copy];
  }
  return self;
}

- (const GURL&)URL {
  return _URL;
}

@end
