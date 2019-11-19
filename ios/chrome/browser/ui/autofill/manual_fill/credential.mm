// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"

#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillCredential () {
  // iVar to backup URL.
  GURL _URL;
}
@end

@implementation ManualFillCredential

- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL {
  self = [super init];
  if (self) {
    _host = [host copy];
    _siteName = [siteName copy];
    _username = [username copy];
    _password = [password copy];
    _URL = URL;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (!object) {
    return NO;
  }
  if (self == object) {
    return YES;
  }
  if (![object isMemberOfClass:[ManualFillCredential class]]) {
    return NO;
  }
  ManualFillCredential* otherObject = (ManualFillCredential*)object;
  if (![otherObject.host isEqual:self.host]) {
    return NO;
  }
  if (![otherObject.username isEqual:self.username]) {
    return NO;
  }
  if (![otherObject.password isEqual:self.password]) {
    return NO;
  }
  if (![otherObject.siteName isEqual:self.siteName]) {
    return NO;
  }
  if (otherObject.URL != self.URL) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return [base::SysUTF8ToNSString(self.URL.spec()) hash] ^
         [self.username hash] ^ [self.password hash];
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@ (%p): username: %@, siteName: %@, host: %@, URL: %@>",
          NSStringFromClass([self class]), self, self.username, self.siteName,
          self.host, base::SysUTF8ToNSString(self.URL.spec())];
}

- (const GURL&)URL {
  return _URL;
}

@end
