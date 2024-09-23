// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

@implementation ManualFillCredential

- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL {
  self = [super initWithSiteName:siteName host:host URL:URL];
  if (self) {
    _username = [username copy];
    _password = [password copy];
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
  if (![otherObject.host isEqualToString:self.host]) {
    return NO;
  }
  if (![otherObject.username isEqualToString:self.username]) {
    return NO;
  }
  if (![otherObject.password isEqualToString:self.password]) {
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

@end
