// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_embedded_reauth_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/bind.h"
#import "base/test/test_future.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/signin/test_embedded_reauth.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_interaction_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kTestPackagedLookupToken = @"test_packaged_lookup_token";
NSString* const kTestReAuthProofToken = @"test_reauth_proof_token";

}  // namespace

// Test fixture for `PasskeyEmbeddedReAuthCoordinator`.
class PasskeyEmbeddedReAuthCoordinatorTest : public PlatformTest {
 protected:
  PasskeyEmbeddedReAuthCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_);
    authentication_service->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

    base_view_controller_ = [[UIViewController alloc] init];
    SetUpBrowser(profile_);
  }

  ~PasskeyEmbeddedReAuthCoordinatorTest() override {
    [coordinator_ stop];
    ios::provider::test::ResetEmbeddedReAuthState();
  }

  void SetUpBrowser(ProfileIOS* profile) {
    [coordinator_ stop];
    coordinator_ = nil;
    browser_ = std::make_unique<TestBrowser>(profile);
    mock_passkey_client_commands_handler_ =
        OCMProtocolMock(@protocol(IOSPasskeyClientCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_passkey_client_commands_handler_
                     forProtocol:@protocol(IOSPasskeyClientCommands)];
  }

  void StartCoordinator(
      webauthn::IOSPasskeyClient::EmbeddedReAuthCallback callback,
      NSString* packaged_lookup_token = kTestPackagedLookupToken) {
    [coordinator_ stop];
    coordinator_ = [[PasskeyEmbeddedReAuthCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
               packagedLookupToken:packaged_lookup_token
                          callback:std::move(callback)];
    [coordinator_ start];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  FakeSystemIdentity* identity_ = nil;
  UIViewController* base_view_controller_ = nil;
  id mock_passkey_client_commands_handler_ = nil;
  PasskeyEmbeddedReAuthCoordinator* coordinator_ = nil;
};

// Tests that completing the embedded ReAuth challenge returns the ReAuth proof
// token and dismisses the coordinator.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest, SuccessReturnsReAuthProofToken) {
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), "test_reauth_proof_token");
  EXPECT_NSEQ(ios::provider::test::GetLastEmbeddedReAuthToken(),
              kTestPackagedLookupToken);
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthTokenType(),
            EmbeddedReAuthTokenType::kPackagedLookupToken);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that the coordinator succeeds when started in an incognito browser by
// using the original profile's `AuthenticationService`.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       SuccessInIncognitoUsesOriginalProfileIdentity) {
  SetUpBrowser(profile_->GetOffTheRecordProfile());
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), "test_reauth_proof_token");
  EXPECT_NSEQ(ios::provider::test::GetLastEmbeddedReAuthToken(),
              kTestPackagedLookupToken);
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthTokenType(),
            EmbeddedReAuthTokenType::kPackagedLookupToken);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that an error from the embedded ReAuth service runs the callback with
// an empty string and dismisses the coordinator.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest, ErrorReturnsEmptyToken) {
  NSError* error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  ios::provider::test::SetEmbeddedReAuthResult(nil, error);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that stopping the coordinator before completion runs the callback with
// an empty string and ignores subsequent completion callbacks.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       StopBeforeCompletionReturnsEmptyToken) {
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  [[mock_passkey_client_commands_handler_ reject] dismissPasskeyEmbeddedReAuth];

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());

  [coordinator_ stop];

  EXPECT_EQ(test_future.Get(), "");
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that starting the coordinator when no primary identity is signed in
// asynchronously returns an empty token and dismisses the coordinator without
// crashing.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       SignedOutProfileReturnsEmptyToken) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_);
  authentication_service->SignOut(signin_metrics::ProfileSignout::kTest);

  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());
  EXPECT_FALSE(test_future.IsReady());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthToken(), nil);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that starting the coordinator when the provider does not support
// Embedded ReAuth asynchronously returns an empty token and dismisses the
// coordinator without calling `InitiateEmbeddedReAuth`.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       UnsupportedProviderReturnsEmptyToken) {
  ios::provider::test::SetEmbeddedReAuthSupported(false);
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());
  EXPECT_FALSE(test_future.IsReady());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthToken(), nil);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that starting the coordinator with an empty Packaged Lookup Token
// asynchronously returns an empty token and dismisses the coordinator.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       EmptyPackagedLookupTokenReturnsEmptyToken) {
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback(), /*packaged_lookup_token=*/@"");
  EXPECT_FALSE(test_future.IsReady());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthToken(), nil);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that starting the coordinator with a nil base view controller
// asynchronously returns an empty token and dismisses the coordinator.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       NilBaseViewControllerReturnsEmptyToken) {
  base_view_controller_ = nil;
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());
  EXPECT_FALSE(test_future.IsReady());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthToken(), nil);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}

// Tests that starting the coordinator when the base view controller is already
// presenting another view controller asynchronously returns an empty token and
// dismisses the coordinator without initiating embedded ReAuth.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       PresentedViewControllerReturnsEmptyToken) {
  id mock_base_view_controller = OCMPartialMock(base_view_controller_);
  UIViewController* presented_view_controller = [[UIViewController alloc] init];
  OCMStub([mock_base_view_controller presentedViewController])
      .andReturn(presented_view_controller);

  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth]);

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(test_future.GetCallback());
  EXPECT_FALSE(test_future.IsReady());

  EXPECT_EQ(test_future.Get(), "");
  EXPECT_EQ(ios::provider::test::GetLastEmbeddedReAuthToken(), nil);
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
  [mock_base_view_controller stopMocking];
}

// Tests that `dismissPasskeyEmbeddedReAuth` stops and releases the coordinator
// before the completion callback runs, so destroying the browser inside the
// callback does not dereference a dangling browser pointer.
TEST_F(PasskeyEmbeddedReAuthCoordinatorTest,
       CallbackDestroyingBrowserDoesNotCrash) {
  ios::provider::test::SetEmbeddedReAuthResult(kTestReAuthProofToken, nil);
  OCMExpect(
      [mock_passkey_client_commands_handler_ dismissPasskeyEmbeddedReAuth])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  base::test::TestFuture<std::string> test_future;
  StartCoordinator(base::BindLambdaForTesting([&](std::string token) {
    EXPECT_EQ(coordinator_, nil);
    browser_.reset();
    test_future.SetValue(std::move(token));
  }));

  EXPECT_EQ(test_future.Get(), "test_reauth_proof_token");
  EXPECT_OCMOCK_VERIFY(mock_passkey_client_commands_handler_);
}
