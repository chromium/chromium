// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_system_identity_details.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeSystemIdentityDetails

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity {
  if ((self = [super init])) {
    _identity = identity;
    DCHECK(_identity);
  }
  return self;
}

@end
