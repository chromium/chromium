// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_identity.h"

#import "ios/web_view/internal/utils/nsobject_description_utils.h"

@implementation CWVIdentity

@synthesize email = _email;
@synthesize fullName = _fullName;
@synthesize gaiaID = _gaiaID;

- (instancetype)initWithEmail:(NSString*)email
                     fullName:(nullable NSString*)fullName
                       gaiaID:(NSString*)gaiaID {
  self = [super init];
  if (self) {
    _email = [email copy];
    _fullName = [fullName copy];
    _gaiaID = [gaiaID copy];
  }
  return self;
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

@end
