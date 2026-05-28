// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credential.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

namespace {

// Returns whether the strings are the same (including if both are nil) or if
// both strings have the same contents.
BOOL stringsAreEqual(NSString* string1, NSString* string2) {
  return string2 == string1 || [string2 isEqualToString:string1];
}

}  // namespace

@implementation ManualFillCredential

- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                     displayName:(NSString*)displayName
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL
              isBackupCredential:(BOOL)isBackupCredential {
  self = [super initWithSiteName:siteName host:host URL:URL];
  if (self) {
    _username = [username copy];
    _password = [password copy];
    _displayName = [displayName copy];
    _isBackupCredential = isBackupCredential;
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
  if (!stringsAreEqual(otherObject.host, self.host)) {
    return NO;
  }
  if (!stringsAreEqual(otherObject.username, self.username)) {
    return NO;
  }
  if (!stringsAreEqual(otherObject.password, self.password)) {
    return NO;
  }
  if (!stringsAreEqual(otherObject.displayName, self.displayName)) {
    return NO;
  }
  if (!stringsAreEqual(otherObject.siteName, self.siteName)) {
    return NO;
  }
  if (otherObject.URL != self.URL) {
    return NO;
  }
  if (otherObject.isBackupCredential != self.isBackupCredential) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return [base::SysUTF8ToNSString(self.URL.spec()) hash] ^
         [self.username hash] ^ [self.password hash] ^ [self.displayName hash] ^
         self.isBackupCredential;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@ (%p): username: %@, displayName: %@, siteName: %@, host: "
          @"%@, URL: %@, isBackupCredential: %d>",
          NSStringFromClass([self class]), self, self.username,
          self.displayName, self.siteName, self.host,
          base::SysUTF8ToNSString(self.URL.spec()), self.isBackupCredential];
}

@end
