// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_store.h"

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "device/base/features.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

extern "C" {
// This is a private Security Framework symbol. It indicates that a query must
// be run on the "syncable" macOS keychain, which is where Secure Enclave keys
// are stored. This test needs it because it tries to erase all credentials
// belonging to the (test-only) keychain access group, and the corresponding
// filter label (kSecAttrAccessGroup) appears to be ineffective *unless*
// kSecAttrNoLegacy is `@YES`. Marked as weak import because the symbol is only
// available in 10.11 or greater.
extern const CFStringRef kSecAttrNoLegacy __attribute__((weak_import));
}

namespace device {

using test::TestCallbackReceiver;

namespace fido {
namespace mac {
namespace {

constexpr char kKeychainAccessGroup[] =
    "EQHXZ8M8AV.com.google.chrome.webauthn.test";
constexpr char kMetadataSecret[] = "supersecret";
constexpr char kOtherMetadataSecret[] = "reallynotsosecret";

constexpr char kRpId[] = "rp.example.com";
const std::vector<uint8_t> kUserId = {10, 11, 12, 13, 14, 15};

// Returns a query to use with Keychain instance methods that returns all
// credentials in the non-legacy keychain that are tagged with the keychain
// access group used in this test.
base::ScopedCFTypeRef<CFMutableDictionaryRef> BaseQuery() {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  base::ScopedCFTypeRef<CFStringRef> access_group_ref(
      base::SysUTF8ToCFStringRef(kKeychainAccessGroup));
  CFDictionarySetValue(query, kSecAttrAccessGroup, access_group_ref.release());
  CFDictionarySetValue(query, kSecAttrNoLegacy, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  return query;
}

// Returns all WebAuthn credentials stored in the keychain, regardless of which
// profile they are associated with. May return a null reference if an error
// occurred.
base::ScopedCFTypeRef<CFArrayRef> QueryAllCredentials() {
  if (__builtin_available(macOS 10.12.2, *)) {
    base::ScopedCFTypeRef<CFArrayRef> items;
    OSStatus status = Keychain::GetInstance().ItemCopyMatching(
        BaseQuery(), reinterpret_cast<CFTypeRef*>(items.InitializeInto()));
    if (status == errSecItemNotFound) {
      // The API returns null, but we should return an empty array instead to
      // distinguish from real errors.
      items = base::ScopedCFTypeRef<CFArrayRef>(
          CFArrayCreate(nullptr, nullptr, 0, nullptr));
    } else if (status != errSecSuccess) {
      OSSTATUS_DLOG(ERROR, status);
    }
    return items;
  }
  NOTREACHED();
  return base::ScopedCFTypeRef<CFArrayRef>(nullptr);
}

// Returns the number of WebAuthn credentials in the keychain (for all
// profiles), or -1 if an error occurs.
ssize_t KeychainItemCount() {
  base::ScopedCFTypeRef<CFArrayRef> items = QueryAllCredentials();
  return items ? CFArrayGetCount(items) : -1;
}

bool ResetKeychain() {
  if (__builtin_available(macOS 10.12.2, *)) {
    OSStatus status = Keychain::GetInstance().ItemDelete(BaseQuery());
    if (status != errSecSuccess && status != errSecItemNotFound) {
      OSSTATUS_DLOG(ERROR, status);
      return false;
    }
    return true;
  }
  NOTREACHED();
  return false;
}

class BrowsingDataDeletionTest : public testing::Test {
 public:
  void SetUp() override {
    authenticator_ = MakeAuthenticator(kMetadataSecret);
    CHECK(authenticator_);
    CHECK(ResetKeychain());
  }

  void TearDown() override { ResetKeychain(); }

 protected:
  CtapMakeCredentialRequest MakeRequest() {
    return CtapMakeCredentialRequest(
        test_data::kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
        PublicKeyCredentialUserEntity(kUserId),
        PublicKeyCredentialParams(
            {{PublicKeyCredentialParams::
                  CredentialInfo() /* defaults to ES-256 */}}));
  }

  std::unique_ptr<TouchIdAuthenticator> MakeAuthenticator(
      std::string profile_metadata_secret) {
    return TouchIdAuthenticator::Create(
        {kKeychainAccessGroup, std::move(profile_metadata_secret)});
  }

  bool MakeCredential() { return MakeCredential(authenticator_.get()); }

  bool MakeCredential(TouchIdAuthenticator* authenticator) {
    TestCallbackReceiver<CtapDeviceResponseCode,
                         base::Optional<AuthenticatorMakeCredentialResponse>>
        callback_receiver;
    authenticator->MakeCredential(MakeRequest(), callback_receiver.callback());
    callback_receiver.WaitForCallback();
    auto result = callback_receiver.TakeResult();
    return std::get<0>(result) == CtapDeviceResponseCode::kSuccess;
  }

  bool DeleteCredentials() { return DeleteCredentials(kMetadataSecret); }
  bool DeleteCredentials(const std::string& metadata_secret) {
    return TouchIdCredentialStore(
               AuthenticatorConfig{kKeychainAccessGroup, metadata_secret})
        .DeleteCredentials(base::Time(), base::Time::Max());
  }

  size_t CountCredentials() { return CountCredentials(kMetadataSecret); }
  size_t CountCredentials(const std::string& metadata_secret) {
    return TouchIdCredentialStore(
               AuthenticatorConfig{kKeychainAccessGroup, metadata_secret})
        .CountCredentials(base::Time(), base::Time::Max());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TouchIdAuthenticator> authenticator_;
};

// All tests are disabled because they need to be codesigned with the
// keychain-access-group entitlement, executed on a Macbook Pro with Touch ID
// running macOS 10.12.2 or later, and require user input (Touch ID).

TEST_F(BrowsingDataDeletionTest, DISABLED_Basic) {
  ASSERT_EQ(0, KeychainItemCount());
  ASSERT_TRUE(MakeCredential());
  ASSERT_EQ(1, KeychainItemCount());

  EXPECT_TRUE(DeleteCredentials());
  EXPECT_EQ(0, KeychainItemCount());
}

TEST_F(BrowsingDataDeletionTest, DISABLED_DifferentProfiles) {
  // Create credentials in two different profiles.
  EXPECT_EQ(0, KeychainItemCount());
  ASSERT_TRUE(MakeCredential());
  auto other_authenticator = MakeAuthenticator(kOtherMetadataSecret);
  ASSERT_TRUE(MakeCredential(other_authenticator.get()));
  ASSERT_EQ(2, KeychainItemCount());

  // Delete credential from the first profile.
  EXPECT_TRUE(DeleteCredentials());
  EXPECT_EQ(1, KeychainItemCount());
  // Only providing the correct secret removes the second credential.
  EXPECT_TRUE(DeleteCredentials());
  EXPECT_EQ(1, KeychainItemCount());
  EXPECT_TRUE(DeleteCredentials(kOtherMetadataSecret));
  EXPECT_EQ(0, KeychainItemCount());
}

TEST_F(BrowsingDataDeletionTest, DISABLED_Count) {
  EXPECT_EQ(0u, CountCredentials());
  EXPECT_EQ(0u, CountCredentials(kOtherMetadataSecret));
  EXPECT_TRUE(MakeCredential());
  EXPECT_EQ(1u, CountCredentials());
  EXPECT_EQ(0u, CountCredentials(kOtherMetadataSecret));

  EXPECT_TRUE(DeleteCredentials());
  EXPECT_EQ(0u, CountCredentials());
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
