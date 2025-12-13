// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/strings/sys_string_conversions.h"
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
                                             NSString* domain) {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:url
                                           serviceName:url
                              registryControlledDomain:domain
                                              username:username
                                                  note:@"note"];
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
  id<Credential> credential1 = TestPasswordCredential(user1, url1, domain1);
  id<Credential> credential2 = TestPasswordCredential(user2, url2, domain2);
  id<Credential> credential3 = TestPasswordCredential(user1, url2, domain2);
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
  id<Credential> credential1 = TestPasswordCredential(user1, url1, domain1);
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
  if (@available(iOS 18.0, *)) {
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
    [defaults
        removeObjectForKey:
            AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled()];
  } else {
    GTEST_SKIP() << "Large Blob requires iOS 18.0+.";
  }
}

}  // namespace credential_provider_extension
