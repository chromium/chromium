// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

@implementation ManualFillPlusAddress

- (instancetype)initWithPlusAddress:(NSString*)plusAddress
                           siteName:(NSString*)siteName
                               host:(NSString*)host
                                URL:(const GURL&)URL {
  self = [super initWithSiteName:siteName host:host URL:URL];
  if (self) {
    _plusAddress = [plusAddress copy];
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
  if (![object isMemberOfClass:[ManualFillPlusAddress class]]) {
    return NO;
  }
  ManualFillPlusAddress* otherObject = (ManualFillPlusAddress*)object;
  if (![otherObject.host isEqualToString:self.host]) {
    return NO;
  }
  if (![otherObject.plusAddress isEqualToString:self.plusAddress]) {
    return NO;
  }
  if (![otherObject.siteName isEqualToString:self.siteName]) {
    return NO;
  }
  if (otherObject.URL != self.URL) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return
      [base::SysUTF8ToNSString(self.URL.spec()) hash] ^ [self.plusAddress hash];
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@ (%p): plusAddress: %@, siteName: %@, host: %@, URL: %@>",
          NSStringFromClass([self class]), self, self.plusAddress,
          self.siteName, self.host, base::SysUTF8ToNSString(self.URL.spec())];
}

@end
