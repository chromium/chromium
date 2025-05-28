// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details+Testing.h"
#import "testing/platform_test.h"

namespace {

NSString* const url1 = @"http://www.example.com";
NSString* const url2 = @"http://www.example2.com";
NSString* const url3 = @"http://www.example2.com";

NSString* const user1 = @"user1";
NSString* const user2 = @"user2";
NSString* const user3 = @"user3";

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

constexpr int64_t kJan1st2024 = 1704085200;

ArchivableCredential* TestPasswordCredential(NSString* username,
                                             NSString* url) {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:url
                                           serviceName:url
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
          lastUsedTime:kJan1st2024];
}

}  // namespace

namespace credential_provider_extension {

class PasskeyRequestDetailsTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void PasskeyRequestDetailsTest::SetUp() {
  if (@available(iOS 17.0, *)) {
  } else {
    GTEST_SKIP() << "Does not apply on iOS 16 and below";
  }
}

void PasskeyRequestDetailsTest::TearDown() {}

// Tests that the allowed credentials list works as expected.
TEST_F(PasskeyRequestDetailsTest, MatchingPassword) {
  id<Credential> credential1 = TestPasswordCredential(user1, url1);
  id<Credential> credential2 = TestPasswordCredential(user2, url1);
  id<Credential> credential3 = TestPasswordCredential(user1, url2);
  id<Credential> credential4 = TestPasskeyCredential(user3, url1);
  NSArray<id<Credential>>* credentials =
      @[ credential1, credential2, credential3, credential4 ];

  // Matching credential 1.
  PasskeyRequestDetails* details =
      [[PasskeyRequestDetails alloc] initWithURL:url1
                                        username:user1
                             excludedCredentials:nil];
  EXPECT_TRUE([details hasMatchingPassword:credentials]);

  // Matching credential 2.
  details = [[PasskeyRequestDetails alloc] initWithURL:url1
                                              username:user2
                                   excludedCredentials:nil];
  EXPECT_TRUE([details hasMatchingPassword:credentials]);

  // Empty credentials list.
  EXPECT_FALSE([details hasMatchingPassword:@[]]);

  // Matching no credential.
  details = [[PasskeyRequestDetails alloc] initWithURL:url2
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
  id<Credential> credential1 = TestPasswordCredential(user1, url1);
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

}  // namespace credential_provider_extension
