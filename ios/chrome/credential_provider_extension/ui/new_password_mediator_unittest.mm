// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake implementation of NewPasswordUIHandler so tests can tell if any UI
// methods were called
@interface FakeNewPasswordUIHandler : NSObject <NewPasswordUIHandler>

// Whether the `-alertUserCredentialExists` method was called.
@property(nonatomic, assign) BOOL alertedCredentialExists;
// Whether the `-alertSavePasswordFailed` method was called.
@property(nonatomic, assign) BOOL alertedSaveFailed;
// Password passed to the consumer.
@property(nonatomic, copy) NSString* password;

@end

@implementation FakeNewPasswordUIHandler

- (void)alertUserCredentialExists {
  self.alertedCredentialExists = YES;
}

- (void)alertSavePasswordFailed {
  self.alertedSaveFailed = YES;
}

- (void)credentialSaved:(ArchivableCredential*)credential {
  // No-op.
}

@end

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

NSString* const testWebsiteBase = @"https://wwww.example.com";
NSString* const testWebsite =
    [NSString stringWithFormat:@"%@/test?page=1", testWebsiteBase];

NSUserDefaults* TestUserDefaults() {
  return [NSUserDefaults standardUserDefaults];
}

ArchivableCredential* TestCredential(NSString* recordIdentifier) {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:5
                                      recordIdentifier:recordIdentifier
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                                              username:@"user"
                                                  note:@"note"];
}

class NewPasswordMediatorTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  ASCredentialServiceIdentifier* serviceIdentifier_ =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:testWebsite
                        type:ASCredentialServiceIdentifierTypeURL];
  NewPasswordMediator* mediator_ =
      [[NewPasswordMediator alloc] initWithUserDefaults:TestUserDefaults()
                                      serviceIdentifier:serviceIdentifier_];
  id<MutableCredentialStore> store_;
  FakeNewPasswordUIHandler* uiHandler_ =
      [[FakeNewPasswordUIHandler alloc] init];
  MockCredentialResponseHandler* responseHandler_ =
      [[MockCredentialResponseHandler alloc] init];
};

void NewPasswordMediatorTest::SetUp() {
  PlatformTest::SetUp();
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  [TestUserDefaults() removeObjectForKey:key];

  store_ = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:TestUserDefaults()
                       key:key];

  mediator_.existingCredentials = store_;
  mediator_.uiHandler = uiHandler_;
  mediator_.credentialResponseHandler = responseHandler_;
}

void NewPasswordMediatorTest::TearDown() {
  PlatformTest::TearDown();
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  [TestUserDefaults() removeObjectForKey:key];
}

// Tests that `-saveNewCredential:completion:` adds a new credential to the
// store and that gets saved to disk.
TEST_F(NewPasswordMediatorTest, SaveNewCredential) {
  // Manually store a credential.
  ArchivableCredential* tempCredential = TestCredential(@"abc");
  [store_ addCredential:tempCredential];
  EXPECT_EQ(1u, store_.credentials.count);
  __block BOOL blockWaitCompleted = NO;
  [store_ saveDataWithCompletion:^(NSError* error) {
    EXPECT_FALSE(error);
    blockWaitCompleted = YES;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));

  // Create a second credential with a new record identifier and make sure it
  // gets saved to disk.
  NSString* testUsername = @"user";
  NSString* testPassword = @"password";
  NSString* testNote = @"note";

  responseHandler_.receivedCredentialBlock = ^() {
    blockWaitCompleted = YES;
  };

  blockWaitCompleted = NO;
  [mediator_ saveCredentialWithUsername:testUsername
                               password:testPassword
                                   note:testNote
                                   gaia:nil
                          shouldReplace:NO];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));

  EXPECT_FALSE(uiHandler_.alertedCredentialExists);
  EXPECT_FALSE(uiHandler_.alertedSaveFailed);

  EXPECT_NSEQ(testUsername, responseHandler_.passwordCredential.user);
  EXPECT_NSEQ(testPassword, responseHandler_.passwordCredential.password);

  // Reload the store from memory and check that the credential was added.
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* freshCredentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:key];
  EXPECT_TRUE(freshCredentialStore);
  EXPECT_TRUE(freshCredentialStore.credentials);
  EXPECT_EQ(2u, freshCredentialStore.credentials.count);
  EXPECT_NSEQ(testUsername, freshCredentialStore.credentials[1].username);
}

// Tests that `-saveNewCredential:completion:` updates an existing credential
// and that gets saved to disk.
TEST_F(NewPasswordMediatorTest, SaveUpdateCredential) {
  // Create a credential that will be stored.
  NSString* recordIdentifier = [NSString
      stringWithFormat:@"%@/test||user||%@/", testWebsiteBase, testWebsiteBase];

  // Create an initial credential with a known record identifier and store that
  // one to disk.
  ArchivableCredential* tempCredential = TestCredential(recordIdentifier);
  [store_ addCredential:tempCredential];
  EXPECT_EQ(1u, store_.credentials.count);
  __block BOOL blockWaitCompleted = NO;
  [store_ saveDataWithCompletion:^(NSError* error) {
    EXPECT_FALSE(error);
    blockWaitCompleted = YES;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));

  // Store the originally created credential and that should update the existing
  // one.
  responseHandler_.receivedCredentialBlock = ^() {
    blockWaitCompleted = YES;
  };

  // The first attempt to save should fail because the user hasn't be notified
  // that their credentials are being replaced.
  blockWaitCompleted = NO;
  NSString* testUsername = @"user";
  NSString* testPassword = @"password";
  NSString* testNote = @"note";
  [mediator_ saveCredentialWithUsername:testUsername
                               password:testPassword
                                   note:testNote
                                   gaia:nil
                          shouldReplace:NO];

  EXPECT_TRUE(uiHandler_.alertedCredentialExists);
  EXPECT_FALSE(uiHandler_.alertedSaveFailed);
  EXPECT_FALSE(blockWaitCompleted);
  uiHandler_.alertedCredentialExists = NO;

  // The second attempt to save should succeed.
  blockWaitCompleted = NO;
  [mediator_ saveCredentialWithUsername:testUsername
                               password:testPassword
                                   note:testNote
                                   gaia:nil
                          shouldReplace:YES];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^BOOL {
    return blockWaitCompleted;
  }));

  EXPECT_FALSE(uiHandler_.alertedCredentialExists);
  EXPECT_FALSE(uiHandler_.alertedSaveFailed);

  EXPECT_NSEQ(testUsername, responseHandler_.passwordCredential.user);
  EXPECT_NSEQ(testPassword, responseHandler_.passwordCredential.password);

  // Reload the store from memory and check that the credential was updated.
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* freshCredentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:key];
  EXPECT_TRUE(freshCredentialStore);
  EXPECT_TRUE(freshCredentialStore.credentials);
  EXPECT_EQ(1u, freshCredentialStore.credentials.count);
  EXPECT_NSEQ(testUsername,
              freshCredentialStore.credentials.firstObject.username);
}

}  // namespace
