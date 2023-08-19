// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/constants.h"

#import <ostream>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

using app_group::ApplicationGroup;

namespace {

// Filename for the archivable storage.
NSString* const kArchivableStorageFilename = @"credential_store";

// Credential Provider dedicated shared folder name.
NSString* const kCredentialProviderContainer = @"credential_provider";

// Used to generate the key for the app group user defaults containing the
// managed user ID to be validated in the extension.
NSString* const kUserDefaultsCredentialProviderManagedUserID =
    @"kUserDefaultsCredentialProviderManagedUserID";

// Used to generate the key for the app group user defaults containing the
// managed user email.
NSString* const kUserDefaultsCredentialProviderManagedUserEmail =
    @"kUserDefaultsCredentialProviderManagedUserEmail";

// Used to generate the key for the app group user defaults containing the
// the metadata for credentials created in the extension.
NSString* const kUserDefaultsCredentialProviderNewCredentials =
    @"kUserDefaultsCredentialProviderNewCredentials";

// Used to generate the key for the app group user defaults containg whether
// saving passwords is currently enabled.
NSString* const kUserDefaulsCredentialProviderSavingPasswordsEnabled =
    @"kUserDefaulsCredentialProviderSavingPasswordsEnabled";

// Used to generate a unique AppGroupPrefix to differentiate between different
// versions of Chrome running in the same device.
NSString* AppGroupPrefix() {
  NSBundle* bundle = base::apple::FrameworkBundle();
  NSDictionary* infoDictionary = bundle.infoDictionary;
  NSString* prefix = infoDictionary[@"MainAppBundleID"];
  if (prefix) {
    return prefix;
  }
  return bundle.bundleIdentifier;
}

}  // namespace

NSURL* CredentialProviderSharedArchivableStoreURL() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];

  // As of 2021Q4, Earl Grey build don't support security groups in their
  // entitlements.
  if (!groupURL) {
    NSBundle* bundle = base::apple::FrameworkBundle();
    NSNumber* isEarlGreyTest =
        [bundle objectForInfoDictionaryKey:@"CRIsEarlGreyTest"];
    if ([isEarlGreyTest boolValue])
      groupURL = [NSURL fileURLWithPath:NSTemporaryDirectory()];
  }

  NSURL* credentialProviderURL =
      [groupURL URLByAppendingPathComponent:kCredentialProviderContainer];
  NSString* filename =
      [AppGroupPrefix() stringByAppendingString:kArchivableStorageFilename];
  return [credentialProviderURL URLByAppendingPathComponent:filename];
}

NSString* AppGroupUserDefaultsCredentialProviderUserID() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderManagedUserID];
}

NSString* AppGroupUserDefaultsCredentialProviderUserEmail() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderManagedUserEmail];
}

NSString* AppGroupUserDefaultsCredentialProviderNewCredentials() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderNewCredentials];
}

NSString* AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaulsCredentialProviderSavingPasswordsEnabled];
}
