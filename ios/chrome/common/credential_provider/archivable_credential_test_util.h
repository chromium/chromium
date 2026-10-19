// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_TEST_UTIL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import <cstdint>
#import <string_view>

@class ArchivableCredential;

// Converts a `std::string_view` to an `NSData` instance for testing.
NSData* TestDataFromString(std::string_view str);

// Default values for `TestPasskeyCredentialOverrides`.
inline NSString* const kDefaultFavicon = @"favicon";
inline NSString* const kDefaultGaia = @"gaia";
inline NSString* const kDefaultRecordIdentifier = @"recordIdentifier";
inline NSString* const kDefaultUsername = @"username";
inline NSString* const kDefaultUserDisplayName = @"userDisplayName";
inline NSString* const kDefaultRpId = @"example.com";
inline constexpr std::string_view kDefaultSyncId = "syncIdOfLength16";
inline constexpr std::string_view kDefaultUserId = "userId";
inline constexpr std::string_view kDefaultCredentialId = "credentialId_16_";
inline constexpr int64_t kDefaultCreationTime = 1704085200000;
inline constexpr int64_t kDefaultLastUsedTime = 1704085200;
inline constexpr int64_t kDefaultHiddenTime = 0;

// Field overrides for constructing a passkey `ArchivableCredential` in tests.
// Any field not explicitly specified uses its default test value above.
struct TestPasskeyCredentialOverrides {
  NSString* favicon = kDefaultFavicon;
  NSString* gaia = kDefaultGaia;
  NSString* record_identifier = kDefaultRecordIdentifier;
  NSData* sync_id = TestDataFromString(kDefaultSyncId);
  NSString* username = kDefaultUsername;
  NSString* user_display_name = kDefaultUserDisplayName;
  NSData* user_id = TestDataFromString(kDefaultUserId);
  NSData* credential_id = TestDataFromString(kDefaultCredentialId);
  NSString* rp_id = kDefaultRpId;
  NSData* private_key = nil;
  NSData* encrypted = nil;
  NSData* security_domain_encrypted = nil;
  int32_t device_auth_key_version = 0;
  int64_t creation_time = kDefaultCreationTime;
  int64_t last_used_time = kDefaultLastUsedTime;
  BOOL hidden = NO;
  int64_t hidden_time = kDefaultHiddenTime;
  BOOL edited_by_user = NO;
};

// Creates a passkey `ArchivableCredential` with default test values and the
// specified `overrides`.
ArchivableCredential* TestPasskeyCredential(
    const TestPasskeyCredentialOverrides& overrides = {});

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_TEST_UTIL_H_
