// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/credential_provider_extension/credential_provider_view_controller+Testing.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* const kTestUserDefaultsKey = @"UserDefaultsCredentialStoreTestKey";
NSString* const kTestRPId = @"example.com";
NSString* const kTestUsername = @"user";
constexpr char kTestUserId[] = "userId";
constexpr char kTestCredentialId[] = "credentialId";

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

NSURL* TestStorageFileURL() {
  return [[NSURL fileURLWithPath:NSTemporaryDirectory()]
      URLByAppendingPathComponent:@"credentials"];
}

ArchivableCredential* TestPasskeyCredential(BOOL edited_by_user = NO) {
  return [[ArchivableCredential alloc]
       initWithFavicon:@"favicon"
                  gaia:nil
      recordIdentifier:@"recordIdentifier"
                syncId:StringToData("syncId")
              username:kTestUsername
       userDisplayName:@"userDisplayName"
                userId:StringToData(kTestUserId)
          credentialId:StringToData(kTestCredentialId)
                  rpId:kTestRPId
            privateKey:StringToData("privateKey")
             encrypted:nil
          creationTime:0
          lastUsedTime:0
                hidden:NO
            hiddenTime:0
          editedByUser:edited_by_user];
}

class CredentialProviderViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    CleanStorage();
    controller_ = [[CredentialProviderViewController alloc] init];
  }

  void TearDown() override {
    CleanStorage();
    PlatformTest::TearDown();
  }

  void CleanStorage() {
    [[NSFileManager defaultManager] removeItemAtURL:TestStorageFileURL()
                                              error:nil];
    NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();
    [userDefaults removeObjectForKey:kTestUserDefaultsKey];
    [userDefaults removeObjectForKey:
                      AppGroupUserDefaulsCredentialProviderSignalAPIEnabled()];
  }

  void SetSignalAPIEnabled() {
    NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
    [defaults setBool:YES
               forKey:AppGroupUserDefaulsCredentialProviderSignalAPIEnabled()];
  }

  void CreateStoreWithCredentials(NSArray<id<Credential>>* credentials) {
    UserDefaultsCredentialStore* userDefaultsStore =
        [[UserDefaultsCredentialStore alloc]
            initWithUserDefaults:app_group::GetGroupUserDefaults()
                             key:kTestUserDefaultsKey];
    ArchivableCredentialStore* archivableStore =
        [[ArchivableCredentialStore alloc]
            initWithFileURL:TestStorageFileURL()];

    for (id<Credential> credential : credentials) {
      [archivableStore addCredential:credential];
    }
    [archivableStore saveDataWithCompletion:nil];

    credential_store_ = [[MultiStoreCredentialStore alloc]
        initWithStores:@[ userDefaultsStore, archivableStore ]];
    controller_.credentialStore = credential_store_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  CredentialProviderViewController* controller_;
  id<CredentialStore> credential_store_;
};

TEST_F(CredentialProviderViewControllerTest,
       ReportUnknownCredentialIgnoredWithFeatureDisabled) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  [controller_
      reportUnknownPublicKeyCredentialForRelyingParty:kTestRPId
                                         credentialID:StringToData(
                                                          kTestCredentialId)];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
}

TEST_F(CredentialProviderViewControllerTest,
       ReportPublicKeyCredentialUpdateIgnoredWithFeatureDisabled) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);

  [controller_
      reportPublicKeyCredentialUpdateForRelyingParty:kTestRPId
                                          userHandle:StringToData(kTestUserId)
                                             newName:@"newUser"];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);
}

TEST_F(CredentialProviderViewControllerTest,
       ReportAllAcceptedPublicKeyCredentialsIgnoredWithFeatureDisabled) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  [controller_
      reportAllAcceptedPublicKeyCredentialsForRelyingParty:kTestRPId
                                                userHandle:StringToData(
                                                               kTestUserId)
                                     acceptedCredentialIDs:@[]];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
}

TEST_F(CredentialProviderViewControllerTest, MarksUnknownCredentialHidden) {
  SetSignalAPIEnabled();
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime, 0);

  [controller_
      reportUnknownPublicKeyCredentialForRelyingParty:kTestRPId
                                         credentialID:StringToData(
                                                          kTestCredentialId)];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime,
            base::Time::Now().InMillisecondsSinceUnixEpoch());
}

TEST_F(CredentialProviderViewControllerTest,
       IgnoresUsernameUpdateForCredentialEditedByUser) {
  SetSignalAPIEnabled();
  CreateStoreWithCredentials(
      @[ TestPasskeyCredential(/*edited_by_user=*/YES) ]);

  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);

  [controller_
      reportPublicKeyCredentialUpdateForRelyingParty:kTestRPId
                                          userHandle:StringToData(kTestUserId)
                                             newName:@"newUser"];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);
}

TEST_F(CredentialProviderViewControllerTest, UpdatesCredentialUsername) {
  SetSignalAPIEnabled();
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);

  [controller_
      reportPublicKeyCredentialUpdateForRelyingParty:kTestRPId
                                          userHandle:StringToData(kTestUserId)
                                             newName:@"newUser"];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"newUser");
}

TEST_F(CredentialProviderViewControllerTest,
       MarksCredentialNotPresentOnAcceptedListAsHidden) {
  SetSignalAPIEnabled();
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  [controller_
      reportAllAcceptedPublicKeyCredentialsForRelyingParty:kTestRPId
                                                userHandle:StringToData(
                                                               kTestUserId)
                                     acceptedCredentialIDs:@[]];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime,
            base::Time::Now().InMillisecondsSinceUnixEpoch());
}

TEST_F(CredentialProviderViewControllerTest, HiddenCredentialRestored) {
  SetSignalAPIEnabled();
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  // Mark the credential as unknown, it should be hidden.
  [controller_
      reportUnknownPublicKeyCredentialForRelyingParty:kTestRPId
                                         credentialID:StringToData(
                                                          kTestCredentialId)];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);

  // After restoring, it shouldn't be hidden anymore.
  [controller_
      reportAllAcceptedPublicKeyCredentialsForRelyingParty:kTestRPId
                                                userHandle:StringToData(
                                                               kTestUserId)
                                     acceptedCredentialIDs:@[
                                       StringToData(kTestCredentialId)
                                     ]];
  EXPECT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime, 0);
}

}  // namespace
