// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

constexpr int64_t kJan1st2024 = 1704085200;

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

ArchivableCredential* TestPasswordCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:1
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"http://www.example.com"
                                           serviceName:nil
                                              username:@"username_value"
                                                  note:@"note"];
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

class CredentialListCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void CredentialListCoordinatorTest::SetUp() {}

void CredentialListCoordinatorTest::TearDown() {}

// Tests that a user selecting a credential will trigger the appropriate
// function in the CredentialResponseHandler.
TEST_F(CredentialListCoordinatorTest, CredentialResponseHandler) {
  base::test::SingleThreadTaskEnvironment task_environment;

  MockReauthenticationModule* reauthenticationModule =
      [[MockReauthenticationModule alloc] init];
  reauthenticationModule.canAttemptWithBiometrics = YES;
  reauthenticationModule.canAttempt = YES;
  reauthenticationModule.expectedResult = ReauthenticationResult::kSuccess;

  ReauthenticationHandler* reauthenticationHandler =
      [[ReauthenticationHandler alloc]
          initWithReauthenticationModule:reauthenticationModule];

  MockCredentialResponseHandler* credentialResponseHandler =
      [[MockCredentialResponseHandler alloc] init];

  CredentialListCoordinator* credentialListCoordinator =
      [[CredentialListCoordinator alloc]
          initWithBaseViewController:nil
                     credentialStore:nil
                  serviceIdentifiers:nil
             reauthenticationHandler:reauthenticationHandler
           credentialResponseHandler:credentialResponseHandler];
  EXPECT_TRUE([credentialListCoordinator
      conformsToProtocol:@protocol(CredentialListUIHandler)]);
  id<CredentialListUIHandler> credentialListUIHandler =
      (id<CredentialListUIHandler>)credentialListCoordinator;

  __block BOOL blockWaitCompleted = NO;
  credentialResponseHandler.receivedCredentialBlock = ^() {
    blockWaitCompleted = YES;
  };

  id<Credential> credential = TestPasswordCredential();
  [credentialListUIHandler userSelectedCredential:credential];
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, true, ^BOOL {
        return blockWaitCompleted;
      }));

  if (@available(iOS 17.0, *)) {
    blockWaitCompleted = NO;
    credentialResponseHandler.receivedCredentialBlock = nil;
    credentialResponseHandler.receivedPasskeyBlock = ^() {
      blockWaitCompleted = YES;
    };

    credential = TestPasskeyCredential();
    [credentialListUIHandler userSelectedCredential:credential];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, true, ^BOOL {
          return blockWaitCompleted;
        }));
  }
}

}  // namespace credential_provider_extension
