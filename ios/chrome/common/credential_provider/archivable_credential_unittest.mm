// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential.h"

#import "base/test/ios/wait_util.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ArchivableCredentialTest = PlatformTest;

ArchivableCredential* TestCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                                  gaia:nil
                                              password:@"qwery123"
                                                  rank:5
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                              registryControlledDomain:@"example.com"
                                              username:@"user"
                                                  note:@"note"
                                          lastUsedTime:0];
}

// Tests that an ArchivableCredential can be created.
TEST_F(ArchivableCredentialTest, create) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                               gaia:nil
                                           password:@"test"
                                               rank:5
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                           registryControlledDomain:@"example.com"
                                           username:@"user"
                                               note:@"note"
                                       lastUsedTime:0];
  EXPECT_TRUE(credential);
  EXPECT_FALSE(credential.isPasskey);
}

// Tests that a passkey ArchivableCredential can be created.
TEST_F(ArchivableCredentialTest, createPasskey) {
  // In a real world scenario, "privateKey", "encrypted", and
  // "securityDomainEncrypted" are mutually exclusive. They use the same
  // internal pointer in the passkey data structure and an internal enum
  // determines which field the data pointer represents. This test verifies that
  // any of them can be nil without issue.

  ArchivableCredential* credential =
      TestPasskeyCredential(TestPasskeyCredentialOverrides{
          .private_key = TestDataFromString("test"),
      });
  EXPECT_TRUE(credential);
  EXPECT_TRUE(credential.isPasskey);

  credential = TestPasskeyCredential(TestPasskeyCredentialOverrides{
      .encrypted = TestDataFromString("test"),
  });
  EXPECT_TRUE(credential);
  EXPECT_TRUE(credential.isPasskey);

  credential = TestPasskeyCredential(TestPasskeyCredentialOverrides{
      .security_domain_encrypted = TestDataFromString("test"),
      .device_auth_key_version = 42,
  });
  EXPECT_TRUE(credential);
  EXPECT_TRUE(credential.isPasskey);
}

// Tests that an ArchivableCredential can be converted to NSData.
TEST_F(ArchivableCredentialTest, createData) {
  ArchivableCredential* credential = TestCredential();
  EXPECT_TRUE(credential);
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);
}

// Tests that a passkey ArchivableCredential can be converted to NSData.
TEST_F(ArchivableCredentialTest, createPasskeyData) {
  ArchivableCredential* credential = TestPasskeyCredential();
  EXPECT_TRUE(credential);
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);
}

// Tests that an ArchivableCredential can be retrieved from NSData.
TEST_F(ArchivableCredentialTest, retrieveData) {
  ArchivableCredential* credential = TestCredential();
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);

  ArchivableCredential* unarchivedCredential =
      [NSKeyedUnarchiver unarchivedObjectOfClass:[ArchivableCredential class]
                                        fromData:data
                                           error:&error];
  EXPECT_TRUE(unarchivedCredential);
  EXPECT_TRUE(
      [unarchivedCredential isKindOfClass:[ArchivableCredential class]]);
  EXPECT_FALSE(unarchivedCredential.isPasskey);

  EXPECT_NSEQ(credential.favicon, unarchivedCredential.favicon);
  EXPECT_NSEQ(credential.password, unarchivedCredential.password);
  EXPECT_EQ(credential.rank, unarchivedCredential.rank);
  EXPECT_NSEQ(credential.recordIdentifier,
              unarchivedCredential.recordIdentifier);
  EXPECT_NSEQ(credential.serviceIdentifier,
              unarchivedCredential.serviceIdentifier);
  EXPECT_NSEQ(credential.serviceName, unarchivedCredential.serviceName);
  EXPECT_NSEQ(credential.registryControlledDomain,
              unarchivedCredential.registryControlledDomain);
  EXPECT_NSEQ(credential.username, unarchivedCredential.username);
}

// Tests that a passkey ArchivableCredential can be retrieved from NSData.
TEST_F(ArchivableCredentialTest, retrievePasskeyData) {
  ArchivableCredential* credential = TestPasskeyCredential();
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);

  ArchivableCredential* unarchivedCredential =
      [NSKeyedUnarchiver unarchivedObjectOfClass:[ArchivableCredential class]
                                        fromData:data
                                           error:&error];
  EXPECT_TRUE(unarchivedCredential);
  EXPECT_TRUE(
      [unarchivedCredential isKindOfClass:[ArchivableCredential class]]);
  EXPECT_TRUE(unarchivedCredential.isPasskey);

  EXPECT_NSEQ(credential.favicon, unarchivedCredential.favicon);
  EXPECT_NSEQ(credential.recordIdentifier,
              unarchivedCredential.recordIdentifier);
  EXPECT_NSEQ(credential.syncId, unarchivedCredential.syncId);
  EXPECT_NSEQ(credential.username, unarchivedCredential.username);
  EXPECT_NSEQ(credential.userDisplayName, unarchivedCredential.userDisplayName);
  EXPECT_NSEQ(credential.userId, unarchivedCredential.userId);
  EXPECT_NSEQ(credential.credentialId, unarchivedCredential.credentialId);
  EXPECT_NSEQ(credential.rpId, unarchivedCredential.rpId);
  EXPECT_NSEQ(credential.privateKey, unarchivedCredential.privateKey);
  EXPECT_NSEQ(credential.encrypted, unarchivedCredential.encrypted);
  EXPECT_NSEQ(credential.securityDomainEncrypted,
              unarchivedCredential.securityDomainEncrypted);
  EXPECT_EQ(credential.deviceAuthKeyVersion,
            unarchivedCredential.deviceAuthKeyVersion);
  EXPECT_EQ(credential.creationTime, unarchivedCredential.creationTime);
  EXPECT_EQ(credential.hidden, unarchivedCredential.hidden);
  EXPECT_EQ(credential.hiddenTime, unarchivedCredential.hiddenTime);
  EXPECT_EQ(credential.editedByUser, unarchivedCredential.editedByUser);
}

// Tests ArchivableCredential equality.
TEST_F(ArchivableCredentialTest, equality) {
  ArchivableCredential* credential = TestCredential();
  ArchivableCredential* credentialIdentical = TestCredential();
  EXPECT_NSEQ(credential, credentialIdentical);
  EXPECT_EQ(credential.hash, credentialIdentical.hash);

  ArchivableCredential* credentialSameIdentifier =
      [[ArchivableCredential alloc] initWithFavicon:@"other_favicon"
                                               gaia:nil
                                           password:@"Qwerty123!"
                                               rank:credential.rank + 10
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"other_serviceIdentifier"
                                        serviceName:@"other_serviceName"
                           registryControlledDomain:@"otherexample.com"
                                           username:@"other_user"
                                               note:@"other_note"
                                       lastUsedTime:0];
  EXPECT_NSNE(credential, credentialSameIdentifier);

  ArchivableCredential* credentialDifferentIdentifier =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                               gaia:nil
                                           password:@"123456789"
                                               rank:credential.rank
                                   recordIdentifier:@"other_recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                           registryControlledDomain:@"otherexample.com"
                                           username:@"user"
                                               note:@"note"
                                       lastUsedTime:0];
  EXPECT_NSNE(credential, credentialDifferentIdentifier);

  EXPECT_NSNE(credential, nil);
}

// Tests ArchivableCredential passkey equality.
TEST_F(ArchivableCredentialTest, passkeyEquality) {
  ArchivableCredential* credential = TestPasskeyCredential();
  ArchivableCredential* credentialIdentical = TestPasskeyCredential();
  EXPECT_NSEQ(credential, credentialIdentical);
  EXPECT_EQ(credential.hash, credentialIdentical.hash);

  ArchivableCredential* credentialSameIdentifier =
      TestPasskeyCredential(TestPasskeyCredentialOverrides{
          .favicon = @"other_favicon",
          .sync_id = TestDataFromString("other_syncId"),
          .username = @"other_username",
          .user_display_name = @"other_userDisplayName",
          .user_id = TestDataFromString("other_userId"),
          .credential_id = TestDataFromString("other_credentialId"),
          .rp_id = @"other_rpId",
          .private_key = TestDataFromString("other_privateKey"),
          .encrypted = TestDataFromString("other_encrypted"),
          .security_domain_encrypted =
              TestDataFromString("other_securityDomainEncrypted"),
          .device_auth_key_version = 43,
          .creation_time = kDefaultCreationTime + 10,
          .last_used_time = kDefaultLastUsedTime + 10,
          .hidden = NO,
          .hidden_time = kDefaultLastUsedTime + 10,
          .edited_by_user = NO,
      });
  EXPECT_NSNE(credential, credentialSameIdentifier);

  ArchivableCredential* credentialDiferentIdentifier =
      TestPasskeyCredential(TestPasskeyCredentialOverrides{
          .record_identifier = @"other_recordIdentifier"});
  EXPECT_NSNE(credential, credentialDiferentIdentifier);

  EXPECT_NSNE(credential, nil);
}

TEST_F(ArchivableCredentialTest, HiddenTimeNotSetForNotHiddenPasskey) {
  ArchivableCredential* credential =
      TestPasskeyCredential(TestPasskeyCredentialOverrides{
          .hidden = NO, .hidden_time = kDefaultLastUsedTime});

  EXPECT_FALSE(PasskeyFromCredential(credential).has_hidden_time());
}

// Tests that `initWithFavicon:gaia:passkey:` and `PasskeyFromCredential`
// preserve `security_domain_encrypted` and `device_authorization_key_version`.
TEST_F(ArchivableCredentialTest, PasskeyFromCredentialSecurityDomainEncrypted) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id("sync_id_16_bytes");
  passkey.set_credential_id("cred_id_16_bytes");
  passkey.set_rp_id("example.com");
  passkey.set_user_id("user_id");
  passkey.set_user_name("username");
  passkey.set_user_display_name("userDisplayName");
  passkey.set_security_domain_encrypted("encrypted_secrets");
  passkey.set_device_authorization_key_version(7);

  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                               gaia:@"gaia"
                                            passkey:passkey];
  ASSERT_TRUE(credential);
  EXPECT_EQ(credential.privateKey.length, 0u);
  EXPECT_EQ(credential.encrypted.length, 0u);
  EXPECT_NSEQ(credential.securityDomainEncrypted,
              TestDataFromString("encrypted_secrets"));
  EXPECT_EQ(credential.deviceAuthKeyVersion, 7);

  sync_pb::WebauthnCredentialSpecifics roundtrip =
      PasskeyFromCredential(credential);
  EXPECT_FALSE(roundtrip.has_private_key());
  EXPECT_FALSE(roundtrip.has_encrypted());
  EXPECT_EQ(roundtrip.security_domain_encrypted(), "encrypted_secrets");
  EXPECT_EQ(roundtrip.device_authorization_key_version(), 7);
}

}  // namespace
