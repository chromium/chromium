// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import <CommonCrypto/CommonCrypto.h>

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

constexpr int64_t kJan1st2024 = 1704085200;

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

NSData* SecurityDomainSecret() {
  std::vector<uint8_t> sds;
  base::HexStringToBytes(
      "1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF", &sds);
  return [NSData dataWithBytes:sds.data() length:sds.size()];
}

ArchivableCredential* TestPasskeyCredential() {
  return
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                               gaia:nil
                                   recordIdentifier:@"recordIdentifier"
                                             syncId:StringToData("syncId")
                                           username:@"username"
                                    userDisplayName:@"userDisplayName"
                                             userId:StringToData("userId")
                                       credentialId:StringToData("credentialId")
                                               rpId:@"rpId"
                                         privateKey:StringToData("privateKey")
                                          encrypted:StringToData("encrypted")
                                       creationTime:kJan1st2024
                                       lastUsedTime:kJan1st2024];
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
                                SecurityDomainSecret());

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
                                SecurityDomainSecret());
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
                                SecurityDomainSecret());
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
                               SecurityDomainSecret());

    ASSERT_NSEQ(clientDataHash, passkeyRegistrationCredential.clientDataHash);
    ASSERT_EQ(passkeyRegistrationCredential.credentialID.length, 16u);
    ASSERT_NSEQ(credential.rpId, passkeyRegistrationCredential.relyingParty);
    ASSERT_NSNE(passkeyRegistrationCredential.attestationObject, nil);
  }
}

}  // namespace credential_provider_extension
