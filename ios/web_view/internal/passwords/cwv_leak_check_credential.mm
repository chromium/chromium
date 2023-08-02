// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_leak_check_credential_internal.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"

@implementation CWVLeakCheckCredential {
  std::unique_ptr<password_manager::LeakCheckCredential> _internalCredential;
}

+ (CWVLeakCheckCredential*)canonicalLeakCheckCredentialWithPassword:
    (CWVPassword*)password {
  // Canonicalization referenced from:
  // components/password_manager/core/browser/ui/credential_utils.h
  auto canonical_credential =
      std::make_unique<password_manager::LeakCheckCredential>(
          password_manager::CanonicalizeUsername(
              password.internalPasswordForm->username_value),
          password.internalPasswordForm->password_value);

  return [[CWVLeakCheckCredential alloc]
      initWithCredential:std::move(canonical_credential)];
}

- (instancetype)initWithCredential:
    (std::unique_ptr<password_manager::LeakCheckCredential>)credential {
  self = [super init];
  DCHECK(credential);
  if (self) {
    _internalCredential = std::move(credential);
  }
  return self;
}

- (const password_manager::LeakCheckCredential&)internalCredential {
  return *_internalCredential;
}

- (NSString*)username {
  return base::SysUTF16ToNSString(_internalCredential->username());
}

- (NSString*)password {
  return base::SysUTF16ToNSString(_internalCredential->password());
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[self class]]) {
    return NO;
  }
  if (object == self) {
    return YES;
  }
  CWVLeakCheckCredential* other = (CWVLeakCheckCredential*)object;
  return [other.username isEqualToString:self.username] &&
         [other.password isEqualToString:self.password];
}

- (NSUInteger)hash {
  return self.username.hash ^ self.password.hash;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  // Object is immutable.
  return self;
}

@end
