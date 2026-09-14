// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_util.h"

#import <string>
#import <string_view>

#import "base/base64.h"
#import "base/containers/flat_map.h"
#import "base/strings/strcat.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "crypto/hash.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::SizeIs;
using ::webauthn::TrustedVaultKeyAvailability;

constexpr std::string_view kTestSalt = "0123456789abcdef";
constexpr std::string_view kTestRaptToken = "test_reauth_proof_token";
constexpr std::string_view kDomainPasskeys = "hw_protected";
constexpr std::string_view kDomainSync = "chromesync";

std::string ExpectedHash(std::string_view data) {
  return base::Base64Encode(crypto::hash::Sha256(data));
}

}  // namespace

using IOSDeviceAuthorizationUtilTest = PlatformTest;

// Tests that an empty request produces bindings containing only the salt hash.
TEST_F(IOSDeviceAuthorizationUtilTest, TestEmptyRequest) {
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  base::flat_map<std::string, std::string> bindings =
      BuildDeviceIntegrityContentBindings(request, kTestSalt);

  EXPECT_THAT(bindings, SizeIs(1));
  EXPECT_EQ(bindings["salt"], ExpectedHash(kTestSalt));
}

// Tests that reauth proof token is hashed with salt and added to bindings.
TEST_F(IOSDeviceAuthorizationUtilTest, TestRequestWithReauthProofToken) {
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  request.set_reauth_proof_token(std::string(kTestRaptToken));

  base::flat_map<std::string, std::string> bindings =
      BuildDeviceIntegrityContentBindings(request, kTestSalt);

  EXPECT_THAT(bindings, SizeIs(2));
  EXPECT_EQ(bindings["salt"], ExpectedHash(kTestSalt));
  EXPECT_EQ(bindings["rapt_token"],
            ExpectedHash(base::StrCat({kTestRaptToken, kTestSalt})));
}

// Tests that trusted vault key availability entries are sorted by security
// domain and formatted with 1-byte boolean before salt.
TEST_F(IOSDeviceAuthorizationUtilTest, TestKeyAvailabilitySortingAndEncoding) {
  sync_pb::GetDeviceAuthorizationKeyRequest request;

  // Add domain "hw_protected" first, then "chromesync".
  // Sorting should order "chromesync" as index 0 and "hw_protected" as index 1.
  TrustedVaultKeyAvailability* passkeys_entry =
      request.add_trusted_vault_key_availability();
  passkeys_entry->set_security_domain(std::string(kDomainPasskeys));
  passkeys_entry->set_key_available(true);

  TrustedVaultKeyAvailability* sync_entry =
      request.add_trusted_vault_key_availability();
  sync_entry->set_security_domain(std::string(kDomainSync));
  sync_entry->set_key_available(false);

  base::flat_map<std::string, std::string> bindings =
      BuildDeviceIntegrityContentBindings(request, kTestSalt);

  EXPECT_THAT(bindings, SizeIs(3));
  EXPECT_EQ(bindings["salt"], ExpectedHash(kTestSalt));

  // Index 0: "chromesync" with false (0x00)
  EXPECT_EQ(bindings["trusted_vault_key_availability_0"],
            ExpectedHash(base::StrCat(
                {kDomainSync, std::string_view("\0", 1), kTestSalt})));

  // Index 1: "hw_protected" with true (0x01)
  EXPECT_EQ(bindings["trusted_vault_key_availability_1"],
            ExpectedHash(base::StrCat(
                {kDomainPasskeys, std::string_view("\1", 1), kTestSalt})));
}

// Tests that all fields (rapt token, key availability, salt) are bound
// correctly together.
TEST_F(IOSDeviceAuthorizationUtilTest, TestFullRequest) {
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  request.set_reauth_proof_token(std::string(kTestRaptToken));

  TrustedVaultKeyAvailability* entry =
      request.add_trusted_vault_key_availability();
  entry->set_security_domain(std::string(kDomainPasskeys));
  entry->set_key_available(true);

  base::flat_map<std::string, std::string> bindings =
      BuildDeviceIntegrityContentBindings(request, kTestSalt);

  EXPECT_THAT(bindings, SizeIs(3));
  EXPECT_EQ(bindings["salt"], ExpectedHash(kTestSalt));
  EXPECT_EQ(bindings["rapt_token"],
            ExpectedHash(base::StrCat({kTestRaptToken, kTestSalt})));
  EXPECT_EQ(bindings["trusted_vault_key_availability_0"],
            ExpectedHash(base::StrCat(
                {kDomainPasskeys, std::string_view("\1", 1), kTestSalt})));
}
