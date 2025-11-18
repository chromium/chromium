// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import <CommonCrypto/CommonCrypto.h>

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "components/cbor/reader.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "device/fido/attestation_object.h"
#import "device/fido/authenticator_data.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/credential_provider_extension/passkey_util_swift.h"
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

NSArray<NSData*>* PRFInputs() {
  NSData* input1 = [@"01234567890123456789012345678901"
      dataUsingEncoding:NSUTF8StringEncoding];
  NSData* input2 = [@"abcdefghijabcdefghijabcdefghijab"
      dataUsingEncoding:NSUTF8StringEncoding];
  return [NSArray arrayWithObjects:input1, input2, nil];
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
              /*trusted_vault_key_version=*/0, /*extension_input_data=*/{},
              /*extension_output_data=*/nullptr);

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

void PasskeyUtilTest::SetUp() {}

void PasskeyUtilTest::TearDown() {}

// Tests assertion returns valid authenticator data.
TEST_F(PasskeyUtilTest, AssertionAuthenticatorDataIsValid) {
  NSData* clientDataHash = ClientDataHash();
  id<Credential> credential = TestPasskeyCredential();

  // An empty allowedCredentials list means all credentials are accepted.
  NSArray<NSData*>* allowedCredentials = [NSArray array];

  // Compute the SHA256 of rpId, which is included in the assertion
  // credential.
  NSRange rpIdRange = NSMakeRange(0, 32);
  NSData* rpIdSha =
      Sha256([credential.rpId dataUsingEncoding:NSUTF8StringEncoding]);

  PasskeyAssertionOutput passkeyAssertionOutput =
      PerformPasskeyAssertion(credential, clientDataHash, allowedCredentials,
                              SecurityDomainSecrets(), /*prf_inputs=*/nil,
                              /*did_complete_uv=*/true);

  EXPECT_NSEQ(clientDataHash, passkeyAssertionOutput.credential.clientDataHash);
  EXPECT_NSEQ(credential.credentialId,
              passkeyAssertionOutput.credential.credentialID);
  EXPECT_NSEQ(credential.rpId, passkeyAssertionOutput.credential.relyingParty);
  EXPECT_NSEQ(credential.userId, passkeyAssertionOutput.credential.userHandle);

  // Verify that the first 32 bytes of the authenticator data are the SHA256
  // of rpId.
  EXPECT_NSEQ([passkeyAssertionOutput.credential.authenticatorData
                  subdataWithRange:rpIdRange],
              rpIdSha);
}

// Tests that values passed as the `did_complete_uv` param for
// PerformPasskeyAssertion are correctly represented in the authenticator data.
TEST_F(PasskeyUtilTest, PerformPasskeyAssertionPropagatesUVBit) {
  for (bool did_complete_uv : std::vector<bool>{true, false}) {
    PasskeyAssertionOutput passkeyAssertionOutput = PerformPasskeyAssertion(
        TestPasskeyCredential(), ClientDataHash(), /*allowed_credentials=*/@[],
        SecurityDomainSecrets(), /*prf_inputs=*/nil, did_complete_uv);

    std::optional<device::AuthenticatorData> auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(
            base::apple::NSDataToSpan(
                passkeyAssertionOutput.credential.authenticatorData));
    ASSERT_TRUE(auth_data.has_value());

    bool uv_flag_value =
        auth_data->flags() &
        static_cast<uint8_t>(
            device::AuthenticatorData::Flag::kTestOfUserVerification);
    EXPECT_EQ(did_complete_uv, uv_flag_value);
  }
}

// Tests assertion fails if the credential is not allowed.
TEST_F(PasskeyUtilTest, AssertionFailsOnCredentialId) {
  NSData* clientDataHash = ClientDataHash();
  id<Credential> credential = TestPasskeyCredential();

  NSArray<NSData*>* allowedCredentials =
      [NSArray arrayWithObject:StringToData("otherCredentialId")];
  PasskeyAssertionOutput passkeyAssertionOutput = PerformPasskeyAssertion(
      credential, clientDataHash, allowedCredentials, SecurityDomainSecrets(),
      /*prf_inputs=*/nil, /*did_complete_uv=*/true);
  EXPECT_NSEQ(passkeyAssertionOutput.credential, nil);
}

// Tests assertion succeeds if the credential is allowed.
TEST_F(PasskeyUtilTest, AssertionSucceedsOnCredentialId) {
  NSData* clientDataHash = ClientDataHash();
  id<Credential> credential = TestPasskeyCredential();

  NSArray<NSData*>* allowedCredentials =
      [NSArray arrayWithObject:credential.credentialId];
  PasskeyAssertionOutput passkeyAssertionOutput = PerformPasskeyAssertion(
      credential, clientDataHash, allowedCredentials, SecurityDomainSecrets(),
      /*prf_inputs=*/nil, /*did_complete_uv=*/true);
  EXPECT_NSNE(passkeyAssertionOutput.credential, nil);
}

// Tests that creating a passkey works properly.
TEST_F(PasskeyUtilTest, CreationSucceeds) {
  NSData* clientDataHash = ClientDataHash();
  id<Credential> credential = TestPasskeyCredential();

  PasskeyCreationOutput passkeyCreationOutput = PerformPasskeyCreation(
      clientDataHash, credential.rpId, credential.username, credential.userId,
      /*gaia=*/nil, SecurityDomainSecrets(), /*prf_inputs=*/nil,
      /*did_complete_uv=*/true);

  EXPECT_NSEQ(clientDataHash, passkeyCreationOutput.credential.clientDataHash);
  EXPECT_EQ(passkeyCreationOutput.credential.credentialID.length, 16u);
  EXPECT_NSEQ(credential.rpId, passkeyCreationOutput.credential.relyingParty);
  EXPECT_NSNE(passkeyCreationOutput.credential.attestationObject, nil);
}

// Tests that values passed as the `did_complete_uv` param for
// PerformPasskeyCreation are correctly represented in the authenticator data.
TEST_F(PasskeyUtilTest, PerformPasskeyCreationPropagatesUVBit) {
  id<Credential> credential = TestPasskeyCredential();

  for (bool did_complete_uv : std::vector<bool>{true, false}) {
    PasskeyCreationOutput passkeyCreationOutput = PerformPasskeyCreation(
        ClientDataHash(), credential.rpId, credential.username,
        credential.userId,
        /*gaia=*/nil, SecurityDomainSecrets(), /*prf_inputs=*/nil,
        /*did_complete_uv=*/did_complete_uv);

    NSData* attestationObjectData =
        passkeyCreationOutput.credential.attestationObject;
    std::optional<cbor::Value> attestation_cbor =
        cbor::Reader::Read(base::apple::NSDataToSpan(attestationObjectData));
    ASSERT_TRUE(attestation_cbor.has_value());

    std::optional<device::AttestationObject> attestation_object =
        device::AttestationObject::Parse(std::move(*attestation_cbor));
    ASSERT_TRUE(attestation_object.has_value());

    const device::AuthenticatorData& auth_data =
        attestation_object->authenticator_data();
    bool uv_flag_value =
        auth_data.flags() &
        static_cast<uint8_t>(
            device::AuthenticatorData::Flag::kTestOfUserVerification);
    EXPECT_EQ(did_complete_uv, uv_flag_value);
  }
}

// Tests assertion succeeds with PRF data.
TEST_F(PasskeyUtilTest, AssertionSucceedsWithPRF) {
  if (@available(iOS 18.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    PasskeyAssertionOutput passkeyAssertionOutput = PerformPasskeyAssertion(
        credential, clientDataHash, /*allowedCredentials=*/nil,
        SecurityDomainSecrets(), PRFInputs(),
        /*did_complete_uv=*/true);
    EXPECT_NSNE(passkeyAssertionOutput.credential, nil);
    ASSERT_EQ(passkeyAssertionOutput.prf_outputs.count, 2u);
    EXPECT_EQ(passkeyAssertionOutput.prf_outputs[0].length, 32u);
    EXPECT_EQ(passkeyAssertionOutput.prf_outputs[1].length, 32u);
  }
}

// Tests that creating a passkey works properly with PRF data.
TEST_F(PasskeyUtilTest, CreationSucceedsWithPRF) {
  if (@available(iOS 18.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> credential = TestPasskeyCredential();

    PasskeyCreationOutput passkeyCreationOutput = PerformPasskeyCreation(
        clientDataHash, credential.rpId, credential.username, credential.userId,
        /*gaia=*/nil, SecurityDomainSecrets(), PRFInputs(),
        /*did_complete_uv=*/true);

    EXPECT_NSEQ(clientDataHash,
                passkeyCreationOutput.credential.clientDataHash);
    EXPECT_EQ(passkeyCreationOutput.credential.credentialID.length, 16u);
    EXPECT_NSEQ(credential.rpId, passkeyCreationOutput.credential.relyingParty);
    EXPECT_NSNE(passkeyCreationOutput.credential.attestationObject, nil);
    ASSERT_EQ(passkeyCreationOutput.prf_outputs.count, 2u);
    EXPECT_EQ(passkeyCreationOutput.prf_outputs[0].length, 32u);
    EXPECT_EQ(passkeyCreationOutput.prf_outputs[1].length, 32u);
  }
}

// Tests that `ShouldPerformUserVerificationForPreference` gives the expected
// result for a diverse set of arguments.
TEST_F(PasskeyUtilTest,
       ShouldPerformUserVerificationForPreferenceGivesExpectedResults) {
  // Cases where user verification should be performed.
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired,
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/NO));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired,
      /*is_biometric_authentication_enabled=*/NO,
      /*is_conditional_create=*/NO));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred,
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/NO));
  EXPECT_TRUE(ShouldPerformUserVerificationForPreference(
      @"invalid preference",  // Falls back to "preferred" preference.
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/NO));

  // Cases where user verification shouldn't be performed.
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred,
      /*is_biometric_authentication_enabled=*/NO,
      /*is_conditional_create=*/NO));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      @"invalid preference",  // Falls back to "preferred" preference.
      /*is_biometric_authentication_enabled=*/NO,
      /*is_conditional_create=*/NO));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged,
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/NO));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged,
      /*is_biometric_authentication_enabled=*/NO,
      /*is_conditional_create=*/NO));

  // Cases where user verification would usually be performed, but not for
  // conditional creation.
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired,
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/YES));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired,
      /*is_biometric_authentication_enabled=*/NO,
      /*is_conditional_create=*/YES));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred,
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/YES));
  EXPECT_FALSE(ShouldPerformUserVerificationForPreference(
      @"invalid preference",  // Falls back to "preferred" preference.
      /*is_biometric_authentication_enabled=*/YES,
      /*is_conditional_create=*/YES));
}

// Tests that the 'setLargeBlobIsSupported' setter works to mark Large Blob
// support in iOS 18.0+.
TEST_F(PasskeyUtilTest, LargeBlobRegistrationIsSupportedWorks) {
  if (@available(iOS 18.0, *)) {
    NSData* clientDataHash = ClientDataHash();
    id<Credential> seed = TestPasskeyCredential();

    PasskeyCreationOutput passkeyCreationOutput = PerformPasskeyCreation(
        clientDataHash, seed.rpId, seed.username, seed.userId,
        /*gaia=*/nil, SecurityDomainSecrets(), /*prf_inputs=*/nil,
        /*did_complete_uv=*/true);
    ASSERT_NSNE(passkeyCreationOutput.credential, nil);
    // By default there should be no Large Blob support marked.
    ASPasskeyRegistrationCredentialExtensionOutput* ext0 =
        passkeyCreationOutput.credential.extensionOutput;
    if ([ext0 respondsToSelector:@selector(largeBlobRegistrationOutput)]) {
      EXPECT_TRUE([ext0 largeBlobRegistrationOutput] == nil ||
                  ![[ext0 largeBlobRegistrationOutput] isSupported]);
    }
    // Mark support and verify it propagates into the extension output.
    [passkeyCreationOutput.credential setLargeBlobIsSupported];
    ASPasskeyRegistrationCredentialExtensionOutput* ext =
        passkeyCreationOutput.credential.extensionOutput;
    ASSERT_NSNE(ext, nil);
    ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput*
        large_blob_output = [ext largeBlobRegistrationOutput];
    ASSERT_NSNE(large_blob_output, nil);
    EXPECT_TRUE([large_blob_output isSupported]);
  } else {
    GTEST_SKIP() << "Large Blob requires iOS 18.0+.";
  }
}
}  // namespace credential_provider_extension
