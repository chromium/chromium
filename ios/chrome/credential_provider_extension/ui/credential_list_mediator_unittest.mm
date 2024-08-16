// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/mock_credential_store.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator+Testing.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/mock_credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/mock_credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

constexpr int64_t kJan1st2024 = 1704085200;

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

ArchivableCredential* TestPasskeyCredential() {
  return [[ArchivableCredential alloc]
       initWithFavicon:@"favicon1"
                  gaia:nil
      recordIdentifier:@"recordIdentifier1"
                syncId:StringToData("syncId1")
              username:@"username1"
       userDisplayName:@"userDisplayName1"
                userId:StringToData("userId1")
          credentialId:StringToData("credentialId1")
                  rpId:@"rpId1"
            privateKey:StringToData("privateKey1")
             encrypted:StringToData("encrypted1")
          creationTime:kJan1st2024
          lastUsedTime:kJan1st2024];
}

ArchivableCredential* TestPasskeyCredential2() {
  return [[ArchivableCredential alloc]
       initWithFavicon:@"favicon2"
                  gaia:nil
      recordIdentifier:@"recordIdentifier2"
                syncId:StringToData("syncId2")
              username:@"username2"
       userDisplayName:@"userDisplayName2"
                userId:StringToData("userId2")
          credentialId:StringToData("credentialId2")
                  rpId:@"rpId2"
            privateKey:StringToData("privateKey2")
             encrypted:StringToData("encrypted2")
          creationTime:kJan1st2024 + 1
          lastUsedTime:kJan1st2024 + 1];
}

ArchivableCredential* TestPasswordCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"http://www.example.com"
                                           serviceName:@"example.com"
                                              username:@"username_value"
                                                  note:@"note"];
}

ArchivableCredential* TestPasswordCredential2() {
  return
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty1234"
                                               rank:2
                                   recordIdentifier:@"recordIdentifier2"
                                  serviceIdentifier:@"http://www.example2.com"
                                        serviceName:@"example2.com"
                                           username:@"username_value2"
                                               note:@"note2"];
}

NSArray<ASCredentialServiceIdentifier*>* ServiceIdentifierWithName(
    NSString* serviceName) {
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:serviceName
                        type:ASCredentialServiceIdentifierTypeDomain];
  return [NSArray arrayWithObject:serviceIdentifier];
}

id<CredentialListUIHandler> UIHandlerWithCredentialId(NSData* credentialId) {
  if (credentialId != nil) {
    NSArray<NSData*>* allowedCredentials =
        [NSArray arrayWithObject:credentialId];
    return [[MockCredentialListUIHandler alloc]
        initWithAllowedCredentials:allowedCredentials
               isRequestingPasskey:YES];
  } else {
    return [[MockCredentialListUIHandler alloc] initWithAllowedCredentials:nil
                                                       isRequestingPasskey:NO];
  }
}

}  // namespace

namespace credential_provider_extension {

class CredentialListMediatorTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void CredentialListMediatorTest::SetUp() {}

void CredentialListMediatorTest::TearDown() {}

// Tests that fetching a password credential works properly.
TEST_F(CredentialListMediatorTest, FetchPasswordCredential) {
  MockCredentialResponseHandler* credentialResponseHandler =
      [[MockCredentialResponseHandler alloc] init];

  ArchivableCredential* credential = TestPasswordCredential();

  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredentialId(nil);

  NSArray<id<Credential>>* credentials = [NSArray arrayWithObject:credential];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  MockCredentialListConsumer* consumer =
      [[MockCredentialListConsumer alloc] init];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc]
                   initWithConsumer:consumer
                          UIHandler:UIHandler
                    credentialStore:credentialStore
                 serviceIdentifiers:ServiceIdentifierWithName(
                                        credential.serviceName)
          credentialResponseHandler:credentialResponseHandler];

  __block BOOL blockWaitCompleted = NO;
  consumer.presentSuggestedCredentialsBlock =
      ^(NSArray<id<Credential>>* suggested, NSArray<id<Credential>>* all,
        BOOL showSearchBar, BOOL showNewPasswordOption) {
        ASSERT_EQ(suggested.count, 1u);
        ASSERT_EQ(all.count, 1u);
        EXPECT_NSEQ(suggested[0], credential);
        EXPECT_NSEQ(all[0], credential);
        EXPECT_TRUE(showSearchBar);
        EXPECT_EQ(showNewPasswordOption, IsPasswordCreationUserEnabled());
        blockWaitCompleted = YES;
      };

  [credentialListMediator fetchCredentials];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));
}

// Tests that fetching a passkey credential works properly.
TEST_F(CredentialListMediatorTest, FetchPasskeyCredential) {
  if (@available(iOS 17.0, *)) {
    MockCredentialResponseHandler* credentialResponseHandler =
        [[MockCredentialResponseHandler alloc] init];

    ArchivableCredential* credential = TestPasskeyCredential();

    id<CredentialListUIHandler> UIHandler =
        UIHandlerWithCredentialId(credential.credentialId);

    NSArray<id<Credential>>* credentials = [NSArray arrayWithObject:credential];
    id<CredentialStore> credentialStore =
        [[MockCredentialStore alloc] initWithCredentials:credentials];

    MockCredentialListConsumer* consumer =
        [[MockCredentialListConsumer alloc] init];

    CredentialListMediator* credentialListMediator =
        [[CredentialListMediator alloc]
                     initWithConsumer:consumer
                            UIHandler:UIHandler
                      credentialStore:credentialStore
                   serviceIdentifiers:nil
            credentialResponseHandler:credentialResponseHandler];

    __block BOOL blockWaitCompleted = NO;
    consumer.presentSuggestedCredentialsBlock =
        ^(NSArray<id<Credential>>* suggested, NSArray<id<Credential>>* all,
          BOOL showSearchBar, BOOL showNewPasswordOption) {
          ASSERT_EQ(suggested.count, 1u);
          ASSERT_EQ(all.count, 1u);
          EXPECT_NSEQ(suggested[0], credential);
          EXPECT_NSEQ(all[0], credential);
          EXPECT_TRUE(showSearchBar);
          EXPECT_FALSE(showNewPasswordOption);
          blockWaitCompleted = YES;
        };

    [credentialListMediator fetchCredentials];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
          return blockWaitCompleted;
        }));
  }
}

// Tests that fetching all credentials works properly.
TEST_F(CredentialListMediatorTest, FetchAllCredentials) {
  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredentialId(nil);

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:TestPasswordCredential()];
  [credentials addObject:TestPasskeyCredential()];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:UIHandler
                                       credentialStore:credentialStore
                                    serviceIdentifiers:nil
                             credentialResponseHandler:nil];

  NSArray<id<Credential>>* allCredentials =
      [credentialListMediator fetchAllCredentials];
  ASSERT_EQ(allCredentials.count, 1u);
  EXPECT_FALSE(allCredentials[0].isPasskey);

  if (@available(iOS 17.0, *)) {
    UIHandler = UIHandlerWithCredentialId(credentials[1].credentialId);
    credentialListMediator =
        [[CredentialListMediator alloc] initWithConsumer:nil
                                               UIHandler:UIHandler
                                         credentialStore:credentialStore
                                      serviceIdentifiers:nil
                               credentialResponseHandler:nil];
    allCredentials = [credentialListMediator fetchAllCredentials];
    ASSERT_EQ(allCredentials.count, 1u);
    EXPECT_TRUE(allCredentials[0].isPasskey);
  }
}

// Tests that filtering passkey credentials works properly.
TEST_F(CredentialListMediatorTest, FilterPasskeyCredentials) {
  if (@available(iOS 17.0, *)) {
    ArchivableCredential* credential = TestPasskeyCredential();
    ArchivableCredential* credential2 = TestPasskeyCredential2();

    id<CredentialListUIHandler> UIHandler =
        UIHandlerWithCredentialId(credential2.credentialId);

    NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
    [credentials addObject:credential];
    [credentials addObject:credential2];
    id<CredentialStore> credentialStore =
        [[MockCredentialStore alloc] initWithCredentials:credentials];

    CredentialListMediator* credentialListMediator =
        [[CredentialListMediator alloc] initWithConsumer:nil
                                               UIHandler:UIHandler
                                         credentialStore:credentialStore
                                      serviceIdentifiers:nil
                               credentialResponseHandler:nil];

    credentialListMediator.allCredentials =
        [credentialListMediator fetchAllCredentials];

    NSArray<id<Credential>>* filteredCredentials =
        [credentialListMediator filterPasskeyCredentials];
    ASSERT_EQ(filteredCredentials.count, 1u);
    EXPECT_NSEQ(filteredCredentials[0], credential2);
  }
}

// Tests that filtering password credentials works properly.
TEST_F(CredentialListMediatorTest, FilterPasswordCredentials) {
  ArchivableCredential* credential = TestPasswordCredential();
  ArchivableCredential* credential2 = TestPasswordCredential2();

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:credential];
  [credentials addObject:credential2];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc]
                   initWithConsumer:nil
                          UIHandler:nil
                    credentialStore:credentialStore
                 serviceIdentifiers:ServiceIdentifierWithName(
                                        credential2.serviceName)
          credentialResponseHandler:nil];

  credentialListMediator.allCredentials =
      [credentialListMediator fetchAllCredentials];

  NSArray<id<Credential>>* filteredCredentials =
      [credentialListMediator filterPasswordCredentials];
  ASSERT_EQ(filteredCredentials.count, 1u);
  EXPECT_NSEQ(filteredCredentials[0], credential2);
}

}  // namespace credential_provider_extension
