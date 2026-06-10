// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
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

ArchivableCredential* TestPasskeyCredential(BOOL hidden = NO) {
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
          lastUsedTime:kJan1st2024
                hidden:hidden
            hiddenTime:kJan1st2024
          editedByUser:NO];
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
          lastUsedTime:kJan1st2024 + 1
                hidden:NO
            hiddenTime:kJan1st2024 + 1
          editedByUser:NO];
}

ArchivableCredential* TestPasswordCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"http://www.example.com"
                                           serviceName:@"example.com"
                              registryControlledDomain:@"example.com"
                                              username:@"username_value"
                                                  note:@"note"
                                          lastUsedTime:0];
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
                           registryControlledDomain:@"example2.com"
                                           username:@"username_value2"
                                               note:@"note2"
                                       lastUsedTime:0];
}

NSArray<ASCredentialServiceIdentifier*>* ServiceIdentifierWithName(
    NSString* serviceName) {
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:serviceName
                        type:ASCredentialServiceIdentifierTypeDomain];
  return [NSArray arrayWithObject:serviceIdentifier];
}

id<CredentialListUIHandler> UIHandlerWithCredential(id<Credential> credential) {
  if (credential.credentialId != nil) {
    NSArray<NSData*>* allowedCredentials =
        [NSArray arrayWithObject:credential.credentialId];
    return [[MockCredentialListUIHandler alloc]
        initWithAllowedCredentials:allowedCredentials
            relyingPartyIdentifier:credential.rpId];
  } else {
    return [[MockCredentialListUIHandler alloc] initWithAllowedCredentials:nil
                                                    relyingPartyIdentifier:nil];
  }
}

}  // namespace

namespace credential_provider_extension {

class CredentialListMediatorTest : public PlatformTest {};

// Tests that fetching a password credential works properly.
TEST_F(CredentialListMediatorTest, FetchPasswordCredential) {
  MockCredentialResponseHandler* credentialResponseHandler =
      [[MockCredentialResponseHandler alloc] init];

  ArchivableCredential* credential = TestPasswordCredential();

  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredential(nil);

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
  MockCredentialResponseHandler* credentialResponseHandler =
      [[MockCredentialResponseHandler alloc] init];

  ArchivableCredential* credential = TestPasskeyCredential();

  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredential(credential);

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
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));
}

// Tests that fetching all credentials works properly.
TEST_F(CredentialListMediatorTest, FetchAllCredentials) {
  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredential(nil);

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

  UIHandler = UIHandlerWithCredential(credentials[1]);
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

// Tests that fetching all credentials works properly when there are both
// passkeys and passwords available for the relying party/service identifiers.
TEST_F(CredentialListMediatorTest, FetchAllCredentialsPasskeysAndPasswords) {
  ArchivableCredential* password_credential_1 = TestPasswordCredential();
  ArchivableCredential* password_credential_2 = TestPasswordCredential2();
  ArchivableCredential* passkey_credential = TestPasskeyCredential();

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:password_credential_1];
  [credentials addObject:password_credential_2];
  [credentials addObject:passkey_credential];
  id<CredentialStore> credential_store =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  id<CredentialListUIHandler> ui_handler =
      UIHandlerWithCredential(passkey_credential);

  CredentialListMediator* credential_list_mediator =
      [[CredentialListMediator alloc]
                   initWithConsumer:nil
                          UIHandler:ui_handler
                    credentialStore:credential_store
                 serviceIdentifiers:ServiceIdentifierWithName(
                                        password_credential_1.serviceName)
          credentialResponseHandler:nil];

  NSArray<id<Credential>>* all_credentials =
      [credential_list_mediator fetchAllCredentials];

  ASSERT_EQ(all_credentials.count, 3u);
  EXPECT_NSEQ(all_credentials[0], password_credential_1);
  EXPECT_NSEQ(all_credentials[1], password_credential_2);
  EXPECT_NSEQ(all_credentials[2], passkey_credential);
}

// Tests that fetching all credentials works properly when there there are only
// passkeys available for the relying party (and no passwords matching the
// service identifiers).
TEST_F(CredentialListMediatorTest, FetchAllCredentialsPasskeysOnly) {
  ArchivableCredential* password_credential = TestPasswordCredential();
  ArchivableCredential* passkey_credential = TestPasskeyCredential();

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:password_credential];
  [credentials addObject:passkey_credential];
  id<CredentialStore> credential_store =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  id<CredentialListUIHandler> ui_handler =
      UIHandlerWithCredential(passkey_credential);

  CredentialListMediator* credential_list_mediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:ui_handler
                                       credentialStore:credential_store
                                    serviceIdentifiers:nil
                             credentialResponseHandler:nil];

  NSArray<id<Credential>>* all_credentials =
      [credential_list_mediator fetchAllCredentials];
  ASSERT_EQ(all_credentials.count, 1u);
  EXPECT_NSEQ(all_credentials[0], passkey_credential);
}

// Tests that fetching all credentials filters out hidden passkeys.
TEST_F(CredentialListMediatorTest, FetchAllCredentialsWithHiddenPasskeys) {
  ArchivableCredential* passkey_credential = TestPasskeyCredential();
  ArchivableCredential* hidden_passkey = TestPasskeyCredential(/*hidden=*/YES);

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:passkey_credential];
  [credentials addObject:hidden_passkey];
  id<CredentialStore> credential_store =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  id<CredentialListUIHandler> ui_handler =
      UIHandlerWithCredential(passkey_credential);

  CredentialListMediator* credential_list_mediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:ui_handler
                                       credentialStore:credential_store
                                    serviceIdentifiers:nil
                             credentialResponseHandler:nil];

  NSArray<id<Credential>>* all_credentials =
      [credential_list_mediator fetchAllCredentials];
  ASSERT_EQ(all_credentials.count, 1u);
  EXPECT_NSEQ(all_credentials[0], passkey_credential);
}

// Tests that filtering passkey credentials works properly.
TEST_F(CredentialListMediatorTest, FilterPasskeyCredentials) {
  ArchivableCredential* credential = TestPasskeyCredential();
  ArchivableCredential* credential2 = TestPasskeyCredential2();

  id<CredentialListUIHandler> UIHandler = UIHandlerWithCredential(credential2);

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
      [credentialListMediator filterCredentials];
  ASSERT_EQ(filteredCredentials.count, 1u);
  EXPECT_NSEQ(filteredCredentials[0], credential2);
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
      [credentialListMediator filterCredentials];
  ASSERT_EQ(filteredCredentials.count, 1u);
  EXPECT_NSEQ(filteredCredentials[0], credential2);
}

// Tests that filtering credentials works properly when both passkeys and
// passwords are available.
TEST_F(CredentialListMediatorTest, FilterPasskeyAndPasswordCredentials) {
  ArchivableCredential* password_credential_1 = TestPasswordCredential();
  ArchivableCredential* password_credential_2 = TestPasswordCredential2();
  ArchivableCredential* passkey_credential = TestPasskeyCredential();

  id<CredentialListUIHandler> ui_handler =
      UIHandlerWithCredential(passkey_credential);

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:password_credential_1];
  [credentials addObject:password_credential_2];
  [credentials addObject:passkey_credential];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  CredentialListMediator* credential_list_mediator =
      [[CredentialListMediator alloc]
                   initWithConsumer:nil
                          UIHandler:ui_handler
                    credentialStore:credentialStore
                 serviceIdentifiers:ServiceIdentifierWithName(
                                        password_credential_1.serviceName)
          credentialResponseHandler:nil];

  credential_list_mediator.allCredentials =
      [credential_list_mediator fetchAllCredentials];

  NSArray<id<Credential>>* filtered_credentials =
      [credential_list_mediator filterCredentials];

  ASSERT_EQ(filtered_credentials.count, 2u);
  EXPECT_NSEQ(filtered_credentials[0], password_credential_1);
  EXPECT_NSEQ(filtered_credentials[1], passkey_credential);
}

// Tests that filtering password credentials works properly for subdomains.
TEST_F(CredentialListMediatorTest, FilterPasswordCredentialsSubdomain) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"http://example.com"
                                        serviceName:@"example.com"
                           registryControlledDomain:@"example.com"
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:credential];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"login.example.com"
                        type:ASCredentialServiceIdentifierTypeDomain];
  NSArray* serviceIdentifiers = [NSArray arrayWithObject:serviceIdentifier];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:nil
                                       credentialStore:credentialStore
                                    serviceIdentifiers:serviceIdentifiers
                             credentialResponseHandler:nil];

  credentialListMediator.allCredentials =
      [credentialListMediator fetchAllCredentials];

  NSArray<id<Credential>>* filteredCredentials =
      [credentialListMediator filterCredentials];
  ASSERT_EQ(filteredCredentials.count, 1u);
  EXPECT_NSEQ(filteredCredentials[0], credential);
}

// Tests that filtering password credentials rejects false matches.
TEST_F(CredentialListMediatorTest, FilterPasswordCredentialsNoFalseMatch) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"http://example.com"
                                        serviceName:@"example.com"
                           registryControlledDomain:@"example.com"
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:credential];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  ASCredentialServiceIdentifier* serviceIdentifier1 =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"evil-example.com"
                        type:ASCredentialServiceIdentifierTypeDomain];

  ASCredentialServiceIdentifier* serviceIdentifier2 =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"example.com.evil"
                        type:ASCredentialServiceIdentifierTypeDomain];

  ASCredentialServiceIdentifier* serviceIdentifier3 =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"evil.com/login?target=example.com"
                        type:ASCredentialServiceIdentifierTypeDomain];

  NSArray* serviceIdentifiers =
      [NSArray arrayWithObjects:serviceIdentifier1, serviceIdentifier2,
                                serviceIdentifier3, nil];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:nil
                                       credentialStore:credentialStore
                                    serviceIdentifiers:serviceIdentifiers
                             credentialResponseHandler:nil];

  credentialListMediator.allCredentials =
      [credentialListMediator fetchAllCredentials];

  NSArray<id<Credential>>* filteredCredentials =
      [credentialListMediator filterCredentials];
  ASSERT_EQ(filteredCredentials.count, 0u);
}

// Tests that an Android-app credential is not suggested to an unrelated
// web origin whose DNS hostname happens to be lexically equal to the Android
// package name, when there's no matching registry controlled domain.
TEST_F(CredentialListMediatorTest,
       FilterAndroidCredentialsRejectsCollidingWebOrigin) {
  ArchivableCredential* androidCredential = [[ArchivableCredential alloc]
               initWithFavicon:nil
                          gaia:nil
                      password:@"password"
                          rank:1
              recordIdentifier:@"android://hash@example.com"
             serviceIdentifier:@"android://hash@example.com"
                   serviceName:@"android://hash@example.com"
      registryControlledDomain:@""
                      username:@"uesrname_value"
                          note:@""
                  lastUsedTime:0];

  NSMutableArray<id<Credential>>* credentials = [NSMutableArray array];
  [credentials addObject:androidCredential];
  id<CredentialStore> credentialStore =
      [[MockCredentialStore alloc] initWithCredentials:credentials];

  // iOS hands the CPE the *page origin's host* as the service identifier.
  // A request for example.com should not be matched.
  ASCredentialServiceIdentifier* requestedSite =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"example.com"
                        type:ASCredentialServiceIdentifierTypeDomain];
  NSArray* serviceIdentifiers = @[ requestedSite ];

  CredentialListMediator* mediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:nil
                                       credentialStore:credentialStore
                                    serviceIdentifiers:serviceIdentifiers
                             credentialResponseHandler:nil];
  mediator.allCredentials = [mediator fetchAllCredentials];
  EXPECT_EQ([mediator filterCredentials].count, 0u);
}

// Tests that password credential matches registry controlled domain correctly.
TEST_F(CredentialListMediatorTest,
       PasswordCredentialMatchesRegistryControlledDomain) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"https://railway.app/"
                                        serviceName:@"railway.app"
                           registryControlledDomain:@"railway.app"
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:nil
                                       credentialStore:nil
                                    serviceIdentifiers:nil
                             credentialResponseHandler:nil];

  // Exact match.
  EXPECT_TRUE([credentialListMediator passwordCredential:credential
                         matchesRegistryControlledDomain:@"railway.app"]);

  // Subdomain match.
  EXPECT_TRUE([credentialListMediator passwordCredential:credential
                         matchesRegistryControlledDomain:@"login.railway.app"]);

  // Attacker website (e.g. attacker app hosted on private public suffix).
  // The test should expect the attacker website to fail to match.
  EXPECT_FALSE([credentialListMediator
                   passwordCredential:credential
      matchesRegistryControlledDomain:@"attacker.up.railway.app"]);

  // False matches.
  EXPECT_FALSE([credentialListMediator passwordCredential:credential
                          matchesRegistryControlledDomain:@"evil-railway.app"]);
  EXPECT_FALSE([credentialListMediator
                   passwordCredential:credential
      matchesRegistryControlledDomain:@"railway.app.evil.com"]);
  EXPECT_FALSE([credentialListMediator passwordCredential:credential
                          matchesRegistryControlledDomain:@"railway.app.evil"]);
  EXPECT_FALSE([credentialListMediator passwordCredential:credential
                          matchesRegistryControlledDomain:@"evil.com"]);

  // Empty registryControlledDomain should not match anything.
  ArchivableCredential* credentialWithEmptyDomain =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"https://railway.app/"
                                        serviceName:@"railway.app"
                           registryControlledDomain:@""
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];
  EXPECT_FALSE([credentialListMediator
                   passwordCredential:credentialWithEmptyDomain
      matchesRegistryControlledDomain:@"railway.app"]);

  // Nil registryControlledDomain should not match anything.
  ArchivableCredential* credentialWithNilDomain =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"https://railway.app/"
                                        serviceName:@"railway.app"
                           registryControlledDomain:nil
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];
  EXPECT_FALSE([credentialListMediator
                   passwordCredential:credentialWithNilDomain
      matchesRegistryControlledDomain:@"railway.app"]);
}

// Tests that fallback password credential matching (when
// registryControlledDomain is empty) matches domain subdomains correctly but
// rejects public suffix / private registry subdomains.
TEST_F(CredentialListMediatorTest,
       PasswordCredentialMatchesFallbackServiceIdentifiers) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                           password:@"qwerty123"
                                               rank:1
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"https://railway.app/"
                                        serviceName:@"railway.app"
                           registryControlledDomain:@""
                                           username:@"username_value"
                                               note:@"note"
                                       lastUsedTime:0];

  CredentialListMediator* credentialListMediator =
      [[CredentialListMediator alloc] initWithConsumer:nil
                                             UIHandler:nil
                                       credentialStore:nil
                                    serviceIdentifiers:nil
                             credentialResponseHandler:nil];

  // Exact matches.
  ASCredentialServiceIdentifier* serviceIdentifierExact =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"railway.app"
                        type:ASCredentialServiceIdentifierTypeDomain];
  EXPECT_TRUE([credentialListMediator
             passwordCredential:credential
      matchesServiceIdentifiers:@[ serviceIdentifierExact ]]);

  // Subdomain matches.
  ASCredentialServiceIdentifier* serviceIdentifierSubdomain =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"login.railway.app"
                        type:ASCredentialServiceIdentifierTypeDomain];
  EXPECT_TRUE([credentialListMediator
             passwordCredential:credential
      matchesServiceIdentifiers:@[ serviceIdentifierSubdomain ]]);

  // Attacker matches (should NOT match!).
  ASCredentialServiceIdentifier* serviceIdentifierAttacker =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"attacker.up.railway.app"
                        type:ASCredentialServiceIdentifierTypeDomain];
  EXPECT_FALSE([credentialListMediator
             passwordCredential:credential
      matchesServiceIdentifiers:@[ serviceIdentifierAttacker ]]);

  // False matches.
  ASCredentialServiceIdentifier* serviceIdentifierEvilSuffix =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"railway.app.evil.com"
                        type:ASCredentialServiceIdentifierTypeDomain];
  EXPECT_FALSE([credentialListMediator
             passwordCredential:credential
      matchesServiceIdentifiers:@[ serviceIdentifierEvilSuffix ]]);
}

}  // namespace credential_provider_extension
