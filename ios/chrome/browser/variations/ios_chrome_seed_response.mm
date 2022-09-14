// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_seed_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IOSChromeSeedResponse

- (instancetype)initWithSignature:(NSString*)signature
                          country:(NSString*)country
                             time:(NSDate*)time
                             data:(NSData*)data
                       compressed:(BOOL)compressed {
  self = [super init];
  if (self) {
    _signature = signature;
    _country = country;
    _time = time;
    _compressed = compressed;
    _data = data;
  }
  return self;
}

@end
