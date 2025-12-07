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
// current user id.
NSString* const kUserDefaultsCredentialProviderUserID =
    @"kUserDefaultsCredentialProviderUserID";

// Used to generate the key for the app group user defaults containing whether
// multiple profiles are currently in use.
NSString* const kUserDefaultsCredentialProviderMultiProfile =
    @"kUserDefaultsCredentialProviderMultiProfile";

// Used to generate the key for the app group user defaults containing the
// current user id.
NSString* const kUserDefaultsCredentialProviderUserEmail =
    @"kUserDefaultsCredentialProviderUserEmail";

// Used to generate the key for the app group user defaults containing the
// the metadata for credentials created in the extension.
NSString* const kUserDefaultsCredentialProviderNewCredentials =
    @"kUserDefaultsCredentialProviderNewCredentials";

// Used to generate the key for the app group user defaults containing whether
// saving passwords is currently enabled.
NSString* const kUserDefaulsCredentialProviderSavingPasswordsEnabled =
    @"kUserDefaulsCredentialProviderSavingPasswordsEnabled";

// Used to generate the key for the app group user defaults containing whether
// saving passwords is currently managed by enterprise policy.
NSString* const kUserDefaultsCredentialProviderSavingPasswordsManaged =
    @"kUserDefaultsCredentialProviderSavingPasswordsManaged";

// Used to generate the key for the app group user defaults containing whether
// saving passkeys is currently allowed by enterprise policy.
NSString* const kUserDefaulsCredentialProviderSavingPasskeysEnabled =
    @"kUserDefaulsCredentialProviderSavingPasskeysEnabled";

// Used to generate the key for the app group user defaults containing whether
// syncing passwords is currently enabled.
NSString* const kUserDefaultsCredentialProviderPasswordSyncSetting =
    @"kUserDefaultsCredentialProviderPasswordSyncSetting";

// Used to generate the key for the app group user defaults containing whether
// automatic passkey upgrade is currently enabled.
NSString* const kUserDefaultsCredentialProviderAutomaticPasskeyUpgradeSetting =
    @"kUserDefaultsCredentialProviderAutomaticPasskeyUpgradeSetting";

// Used to generate the key for the app group user defaults containing whether
// passkey PRF support is currently enabled.
NSString* const kUserDefaultsCredentialProviderPasskeyPRFSetting =
    @"kUserDefaultsCredentialProviderPasskeyPRFSetting";

// Used to generate the key for the app group user defaults containing whether
// passkey Large Blob support is currently enabled.
NSString* const kUserDefaultsCredentialProviderPasskeyLargeBlobSetting =
    @"kUserDefaultsCredentialProviderPasskeyLargeBlobSetting";

// Used to generate the key for the app group user defaults containing whether
// signal API is currently enabled.
NSString* const kUserDefaultsCredentialProviderSignalAPISetting =
    @"kUserDefaultsCredentialProviderSignalAPISetting";

// Used to generate the key for the app group user defaults containing whether
// the button order in the confirmation alerts should be swapped.
NSString* const
    kUserDefaultsCredentialProviderConfirmationButtonSwapOrderSetting =
        @"ConfirmationButtonSwapOrderKey";

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
    if ([isEarlGreyTest boolValue]) {
      groupURL = [NSURL fileURLWithPath:NSTemporaryDirectory()];
    }
  }

  // Outside of Earl Grey tests,
  // containerURLForSecurityApplicationGroupIdentifier: should not return nil.
  CHECK(groupURL);

  NSURL* credentialProviderURL =
      [groupURL URLByAppendingPathComponent:kCredentialProviderContainer];
  NSString* filename =
      [AppGroupPrefix() stringByAppendingString:kArchivableStorageFilename];
  return [credentialProviderURL URLByAppendingPathComponent:filename];
}

NSString* AppGroupUserDefaultsCredentialProviderManagedUserID() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderManagedUserID];
}

NSString* AppGroupUserDefaultsCredentialProviderUserID() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderUserID];
}

NSString* AppGroupUserDefaultsCredentialProviderMultiProfileSetting() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderMultiProfile];
}

NSString* AppGroupUserDefaultsCredentialProviderUserEmail() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderUserEmail];
}

NSString* AppGroupUserDefaultsCredentialProviderNewCredentials() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderNewCredentials];
}

NSString* AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaulsCredentialProviderSavingPasswordsEnabled];
}

NSString* AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaultsCredentialProviderSavingPasswordsManaged];
}

NSString* AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaulsCredentialProviderSavingPasskeysEnabled];
}

NSString* AppGroupUserDefaultsCredentialProviderPasswordSyncSetting() {
  return
      [AppGroupPrefix() stringByAppendingString:
                            kUserDefaultsCredentialProviderPasswordSyncSetting];
}

NSString*
AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaultsCredentialProviderAutomaticPasskeyUpgradeSetting];
}

NSString* AppGroupUserDefaulsCredentialProviderPasskeyPRFEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderPasskeyPRFSetting];
}

NSString* AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaultsCredentialProviderPasskeyLargeBlobSetting];
}

NSString* AppGroupUserDefaulsCredentialProviderSignalAPIEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderSignalAPISetting];
}

NSString*
AppGroupUserDefaulsCredentialProviderConfirmationButtonSwapOrderEnabled() {
  return [AppGroupPrefix()
      stringByAppendingString:
          kUserDefaultsCredentialProviderConfirmationButtonSwapOrderSetting];
}
