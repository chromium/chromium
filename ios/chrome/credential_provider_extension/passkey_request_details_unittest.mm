// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details+Testing.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

NSString* const url1 = @"http://www.example.com";
NSString* const url2 = @"http://www.example2.com";
NSString* const url3 = @"http://www.example3.com";

NSString* const domain1 = @"example.com";
NSString* const domain2 = @"example2.com";
NSString* const domain3 = @"example3.com";

NSString* const user1 = @"user1";
NSString* const user2 = @"user2";
NSString* const user3 = @"user3";

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

constexpr int64_t kJan1st2024 = 1704085200;

ArchivableCredential* TestPasswordCredential(NSString* username,
                                             NSString* url,
                                             NSString* domain,
                                             int64_t lastUsedTime) {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:url
                                           serviceName:url
                              registryControlledDomain:domain
                                              username:username
                                                  note:@"note"
                                          lastUsedTime:lastUsedTime];
}

ArchivableCredential* TestPasskeyCredential(NSString* username,
                                            NSString* rpId) {
  return [[ArchivableCredential alloc]
       initWithFavicon:@"favicon"
                  gaia:nil
      recordIdentifier:@"recordIdentifier"
                syncId:StringToData("syncId")
              username:username
       userDisplayName:@"userDisplayName"
                userId:StringToData("userId")
          credentialId:StringToData(base::SysNSStringToUTF8(username))
                  rpId:rpId
            privateKey:StringToData("privateKey")
             encrypted:StringToData("encrypted")
          creationTime:kJan1st2024
          lastUsedTime:kJan1st2024
                hidden:NO
            hiddenTime:kJan1st2024
          editedByUser:NO];
}

}  // namespace

namespace credential_provider_extension {

class PasskeyRequestDetailsTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void PasskeyRequestDetailsTest::SetUp() {}

void PasskeyRequestDetailsTest::TearDown() {}

// Tests that the allowed credentials list works as expected.
TEST_F(PasskeyRequestDetailsTest, MatchingPassword) {
  int64_t recentTime =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();

  id<Credential> credential1 =
      TestPasswordCredential(user1, url1, domain1, recentTime);
  id<Credential> credential2 =
      TestPasswordCredential(user2, url2, domain2, recentTime);
  id<Credential> credential3 =
      TestPasswordCredential(user1, url2, domain2, recentTime);
  id<Credential> credential4 = TestPasskeyCredential(user3, url1);
  NSArray<id<Credential>>* credentials =
      @[ credential1, credential2, credential3, credential4 ];

  // Matching credential 1.
  PasskeyRequestDetails* details =
      [[PasskeyRequestDetails alloc] initWithURL:@"www.example.com"
                                        username:user1
                             excludedCredentials:nil];
  EXPECT_TRUE([details hasMatchingPassword:credentials]);

  // Matching credential 2.
  details = [[PasskeyRequestDetails alloc] initWithURL:@"www.abc.example2.com"
                                              username:user2
                                   excludedCredentials:nil];
  EXPECT_TRUE([details hasMatchingPassword:credentials]);

  // Private Registry boundary and hijack checks.
  id<Credential> credentialRailway = TestPasswordCredential(
      user1, @"https://railway.app/", @"railway.app", recentTime);
  NSArray<id<Credential>>* credentialsWithRailway =
      [credentials arrayByAddingObject:credentialRailway];

  // Standard subdomain should match.
  details = [[PasskeyRequestDetails alloc] initWithURL:@"www.login.railway.app"
                                              username:user1
                                   excludedCredentials:nil];
  EXPECT_TRUE([details hasMatchingPassword:credentialsWithRailway]);

  // Rogue subdomain crossing private suffix boundary should NOT match!
  details =
      [[PasskeyRequestDetails alloc] initWithURL:@"attacker.up.railway.app"
                                        username:user1
                             excludedCredentials:nil];
  EXPECT_FALSE([details hasMatchingPassword:credentialsWithRailway]);

  // False suffix match should NOT match!
  details = [[PasskeyRequestDetails alloc] initWithURL:@"evil-railway.app"
                                              username:user1
                                   excludedCredentials:nil];
  EXPECT_FALSE([details hasMatchingPassword:credentialsWithRailway]);

  // Empty credentials list.
  EXPECT_FALSE([details hasMatchingPassword:@[]]);

  // Matching no credential.
  details = [[PasskeyRequestDetails alloc] initWithURL:@"www.example23.com"
                                              username:user2
                                   excludedCredentials:nil];
  EXPECT_FALSE([details hasMatchingPassword:credentials]);

  // Matching passkey credential.
  details = [[PasskeyRequestDetails alloc] initWithURL:url1
                                              username:user3
                                   excludedCredentials:nil];
  EXPECT_FALSE([details hasMatchingPassword:credentials]);
}

// Tests that the excluded credentials list works as expected.
TEST_F(PasskeyRequestDetailsTest, ExcludedPasskey) {
  id<Credential> credential1 =
      TestPasswordCredential(user1, url1, domain1, /*lastUsedTime=*/0);
  id<Credential> credential2 = TestPasskeyCredential(user2, url2);
  id<Credential> credential3 = TestPasskeyCredential(user3, url3);
  NSData* id2 = credential2.credentialId;
  NSData* id3 = credential3.credentialId;
  NSArray<id<Credential>>* credentials =
      @[ credential1, credential2, credential3 ];

  // Matching password credential.
  PasskeyRequestDetails* details =
      [[PasskeyRequestDetails alloc] initWithURL:url1
                                        username:user1
                             excludedCredentials:@[ id2, id3 ]];
  EXPECT_FALSE([details hasExcludedPasskey:credentials]);

  // Matching url for credential 2.
  details = [[PasskeyRequestDetails alloc] initWithURL:url2
                                              username:user1
                                   excludedCredentials:@[ id2 ]];
  EXPECT_TRUE([details hasExcludedPasskey:credentials]);

  // Matching url for credential 3.
  details = [[PasskeyRequestDetails alloc] initWithURL:url3
                                              username:user1
                                   excludedCredentials:@[ id3 ]];
  EXPECT_TRUE([details hasExcludedPasskey:credentials]);

  // Empty credentials list.
  EXPECT_FALSE([details hasExcludedPasskey:@[]]);

  // Matching no credential.
  details = [[PasskeyRequestDetails alloc] initWithURL:url1
                                              username:user3
                                   excludedCredentials:@[ id2, id3 ]];
  EXPECT_FALSE([details hasExcludedPasskey:credentials]);
}

TEST_F(PasskeyRequestDetailsTest, LargeBlobHelperDetectsRequest) {
  NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
  [defaults
      setBool:YES
       forKey:AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled()];
  [defaults synchronize];

  // Large Blob required.
  id mockInputRequired =
      OCMClassMock([ASPasskeyRegistrationCredentialExtensionInput class]);
  id mockLargeBlobRequired = OCMClassMock(
      [ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput class]);
  OCMStub([mockLargeBlobRequired supportRequirement])
      .andReturn(
          ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementRequired);
  OCMStub([mockInputRequired largeBlob]).andReturn(mockLargeBlobRequired);
  EXPECT_TRUE([PasskeyRequestDetails
      isLargeBlobSupportRequestedFromRegistrationInput:mockInputRequired]);

  // Large Blob preferred.
  id mockInputPreferred =
      OCMClassMock([ASPasskeyRegistrationCredentialExtensionInput class]);
  id mockLargeBlobPreferred = OCMClassMock(
      [ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput class]);
  OCMStub([mockLargeBlobPreferred supportRequirement])
      .andReturn(
          ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementPreferred);
  OCMStub([mockInputPreferred largeBlob]).andReturn(mockLargeBlobPreferred);
  EXPECT_TRUE([PasskeyRequestDetails
      isLargeBlobSupportRequestedFromRegistrationInput:mockInputPreferred]);

  // Large Blob preference none.
  id mockInputNil =
      OCMClassMock([ASPasskeyRegistrationCredentialExtensionInput class]);
  OCMStub([mockInputNil largeBlob]).andReturn(nil);
  EXPECT_FALSE([PasskeyRequestDetails
      isLargeBlobSupportRequestedFromRegistrationInput:mockInputNil]);

  // Clean up flag.
  [defaults removeObjectForKey:
                AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled()];
}

// Tests that the 5-minute time constraint works as expected for passwords.
TEST_F(PasskeyRequestDetailsTest, MatchingPasswordTimeConstraints) {
  base::Time now = base::Time::Now();

  int64_t validTime = now.ToDeltaSinceWindowsEpoch().InMicroseconds();
  int64_t expiredTime =
      (now - base::Minutes(6)).ToDeltaSinceWindowsEpoch().InMicroseconds();

  id<Credential> validCred =
      TestPasswordCredential(user1, url1, domain1, validTime);
  id<Credential> expiredCred =
      TestPasswordCredential(user2, url2, domain2, expiredTime);
  id<Credential> neverUsedCred =
      TestPasswordCredential(user3, url3, domain3, /*lastUsedTime=*/0);

  // Recent credential should match.
  PasskeyRequestDetails* detailsValid =
      [[PasskeyRequestDetails alloc] initWithURL:domain1
                                        username:user1
                             excludedCredentials:nil];
  EXPECT_TRUE([detailsValid hasMatchingPassword:@[ validCred ]]);

  // Expired credential should not match.
  PasskeyRequestDetails* detailsExpired =
      [[PasskeyRequestDetails alloc] initWithURL:domain2
                                        username:user2
                             excludedCredentials:nil];
  EXPECT_FALSE([detailsExpired hasMatchingPassword:@[ expiredCred ]]);

  // Never used credential should not match.
  PasskeyRequestDetails* detailsNeverUsed =
      [[PasskeyRequestDetails alloc] initWithURL:domain3
                                        username:user3
                             excludedCredentials:nil];
  EXPECT_FALSE([detailsNeverUsed hasMatchingPassword:@[ neverUsedCred ]]);
}

}  // namespace credential_provider_extension
