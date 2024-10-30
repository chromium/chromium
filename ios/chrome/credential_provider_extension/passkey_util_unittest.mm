// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import <CommonCrypto/CommonCrypto.h>

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

void Append(std::vector<uint8_t>& container, NSData* data) {
  base::span<const uint8_t> span = base::apple::NSDataToSpan(data);
  container.insert(container.end(), span.begin(), span.end());
}

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

NSData* Sha256(NSData* data) {
  NSMutableData* mac_out =
      [NSMutableData dataWithLength:CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(data.bytes, data.length,
            static_cast<unsigned char*>(mac_out.mutableBytes));
  return mac_out;
}

NSData* ClientDataHash() {
  return Sha256(StringToData("ClientDataHash"));
}

NSArray<NSData*>* SecurityDomainSecrets() {
  std::vector<uint8_t> sds;
  base::HexStringToBytes(
      "1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF", &sds);
  return [NSArray arrayWithObjects:[NSData dataWithBytes:sds.data()
                                                  length:sds.size()],
                                   nil];
}

ArchivableCredential* TestPasskeyCredential() {
  std::vector<uint8_t> trusted_vault_key;
  NSArray<NSData*>* security_domain_secrets = SecurityDomainSecrets();
  Append(trusted_vault_key, security_domain_secrets[0]);

  std::vector<uint8_t> user_id;
  Append(user_id, StringToData("userId"));
  std::string rp_id_str("rpId");
  std::string user_name_str("username");

  // Generate a key pair containing the webauthn specifics and the public key.
  std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
      generated_passkey =
          webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
              rp_id_str,
              webauthn::PasskeyModel::UserEntity(user_id, user_name_str,
                                                 user_name_str),
              trusted_vault_key,
              /*trusted_vault_key_version=*/0);

  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                               passkey:generated_passkey.first];
}

}  // namespace

namespace credential_provider_extension {

class PasskeyUtilTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void PasskeyUtilTest::SetUp() {
  if (@available(iOS 17.0, *)) {
  } else {
    GTEST_SKIP() << "Does not apply on iOS 16 and below";
  }
}

void PasskeyUtilTest::TearDown() {}

// Tests assertion returns valid authenticator data.
TEST_F(PasskeyUtilTest, AssertionAuthenticatorDataIsValid) {
  if (@available(iOS 17.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    // An empty allowedCredentials list means all credentials are accepted.
    NSArray<NSData*>* allowedCredentials = [NSArray array];

    // Compute the SHA256 of rpId, which is included in the assertion
    // credential.
    NSRange rpIdRange = NSMakeRange(0, 32);
    NSData* rpIdSha =
        Sha256([credential.rpId dataUsingEncoding:NSUTF8StringEncoding]);

    ASPasskeyAssertionCredential* passkeyAssertionCredential =
        PerformPasskeyAssertion(credential, clientDataHash, allowedCredentials,
                                SecurityDomainSecrets());

    ASSERT_NSEQ(clientDataHash, passkeyAssertionCredential.clientDataHash);
    ASSERT_NSEQ(credential.credentialId,
                passkeyAssertionCredential.credentialID);
    ASSERT_NSEQ(credential.rpId, passkeyAssertionCredential.relyingParty);
    ASSERT_NSEQ(credential.userId, passkeyAssertionCredential.userHandle);

    // Verify that the first 32 bytes of the authenticator data are the SHA256
    // of rpId.
    ASSERT_NSEQ([passkeyAssertionCredential.authenticatorData
                    subdataWithRange:rpIdRange],
                rpIdSha);
  }
}

// Tests assertion fails if the credential is not allowed.
TEST_F(PasskeyUtilTest, AssertionFailsOnCredentialId) {
  if (@available(iOS 17.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    NSArray<NSData*>* allowedCredentials =
        [NSArray arrayWithObject:StringToData("otherCredentialId")];
    ASPasskeyAssertionCredential* passkeyAssertionCredential =
        PerformPasskeyAssertion(credential, clientDataHash, allowedCredentials,
                                SecurityDomainSecrets());
    ASSERT_NSEQ(passkeyAssertionCredential, nil);
  }
}

// Tests assertion succeeds if the credential is allowed.
TEST_F(PasskeyUtilTest, AssertionSucceedsOnCredentialId) {
  if (@available(iOS 17.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    NSArray<NSData*>* allowedCredentials =
        [NSArray arrayWithObject:credential.credentialId];
    ASPasskeyAssertionCredential* passkeyAssertionCredential =
        PerformPasskeyAssertion(credential, clientDataHash, allowedCredentials,
                                SecurityDomainSecrets());
    ASSERT_NSNE(passkeyAssertionCredential, nil);
  }
}

// Tests that creating a passkey works properly.
TEST_F(PasskeyUtilTest, CreationSucceeds) {
  if (@available(iOS 17.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    ASPasskeyRegistrationCredential* passkeyRegistrationCredential =
        PerformPasskeyCreation(clientDataHash, credential.rpId,
                               credential.username, credential.userId, nil,
                               SecurityDomainSecrets());

    ASSERT_NSEQ(clientDataHash, passkeyRegistrationCredential.clientDataHash);
    ASSERT_EQ(passkeyRegistrationCredential.credentialID.length, 16u);
    ASSERT_NSEQ(credential.rpId, passkeyRegistrationCredential.relyingParty);
    ASSERT_NSNE(passkeyRegistrationCredential.attestationObject, nil);
  }
}

// Tests that `ShouldPerformUserVerificationForPreference` gives the expected
// result for a diverse set of arguments.
TEST_F(PasskeyUtilTest,
       ShouldPerformUserVerificationForPreferenceGivesExpectedResults) {
  // Cases where user verification should be performed.
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"required", /*is_biometric_authentication_enabled=*/YES));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"required", /*is_biometric_authentication_enabled=*/NO));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"preferred", /*is_biometric_authentication_enabled=*/YES));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"Discouraged",
      /*is_biometric_authentication_enabled=*/YES));  // Not a valid preference
                                                      // value.
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"Discouraged",
      /*is_biometric_authentication_enabled=*/NO));  // Not a valid preference
                                                     // value.

  // Cases where user verification shouldn't be performed.
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      @"preferred", /*is_biometric_authentication_enabled=*/NO));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      @"discouraged", /*is_biometric_authentication_enabled=*/YES));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      @"discouraged", /*is_biometric_authentication_enabled=*/NO));
}

}  // namespace credential_provider_extension
