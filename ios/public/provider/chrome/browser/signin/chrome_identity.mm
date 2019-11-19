// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeIdentity

- (NSString*)userEmail {
  NOTREACHED();
  return nil;
}

- (NSString*)gaiaID {
  NOTREACHED();
  return nil;
}

- (NSString*)userFullName {
  NOTREACHED();
  return nil;
}

- (NSString*)hashedGaiaID {
  NOTREACHED();
  return nil;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, GaiaID: \"%@\", name: \"%@\", "
                                    @"email: \"%@\">",
                                    self.class.description, self, self.gaiaID,
                                    self.userFullName, self.userEmail];
}

@end
