// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"

#import "ios/components/credential_provider_extension/password_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVCredentialProviderExtensionUtils

+ (nullable NSString*)retrievePasswordForKeychainIdentifier:
    (NSString*)keychainIdentifier {
  return credential_provider_extension::PasswordWithKeychainIdentifier(
      keychainIdentifier);
}

+ (BOOL)storePasswordForKeychainIdentifier:(NSString*)keychainIdentifier
                                  password:(NSString*)password {
  return credential_provider_extension::StorePasswordInKeychain(
      password, keychainIdentifier);
}

@end
