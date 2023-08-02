// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/password_util.h"

#import "base/logging.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/components/credential_provider_extension/password_util.h"

NSString* PasswordWithKeychainIdentifier(NSString* identifier) {
  if (!identifier) {
    UpdateUMACountForKey(
        app_group::kCredentialExtensionFetchPasswordNilArgumentCount);
    return @"";
  }

  NSString* password =
      credential_provider_extension::PasswordWithKeychainIdentifier(identifier);
  if (password) {
    return password;
  }

  UpdateUMACountForKey(
      app_group::kCredentialExtensionFetchPasswordFailureCount);
  return @"";
}

BOOL StorePasswordInKeychain(NSString* password, NSString* identifier) {
  if (!identifier || identifier.length == 0) {
    return NO;
  }

  BOOL stored_successfully =
      credential_provider_extension::StorePasswordInKeychain(password,
                                                             identifier);

  if (!stored_successfully) {
    UpdateUMACountForKey(
        app_group::kCredentialExtensionKeychainSavePasswordFailureCount);
  }

  return stored_successfully;
}
