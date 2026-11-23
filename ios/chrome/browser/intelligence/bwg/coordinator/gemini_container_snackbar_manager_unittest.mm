// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_snackbar_manager.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class GeminiContainerSnackbarManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindOnce(&IdentityTestEnvironmentBrowserStateAdaptor::
                           BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    scene_state_ = [[SceneState alloc] init];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    shown_messages_ = [NSMutableArray array];
    NSMutableArray<SnackbarMessage*>* shown_messages = shown_messages_;
    OCMStub([mock_snackbar_handler_
        showSnackbarMessage:[OCMArg
                                checkWithBlock:^BOOL(SnackbarMessage* message) {
                                  if (message) {
                                    [shown_messages addObject:message];
                                  }
                                  return YES;
                                }]]);
    manager_ =
        [[GeminiContainerSnackbarManager alloc] initWithBrowser:browser_.get()
                                          authenticationService:auth_service_];
    manager_.snackbarHandler = mock_snackbar_handler_;
  }

  void TearDown() override {
    [manager_ disconnect];
    manager_ = nil;
    browser_.reset();
    scene_state_ = nil;
    PlatformTest::TearDown();
  }

  void SignInFakeIdentity() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager())
        ->AddIdentity(identity);

    signin::AccountAvailabilityOptionsBuilder builder;
    builder.WithGaiaId(identity.gaiaId)
        .AsPrimary(signin::ConsentLevel::kSignin);
    signin::MakeAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

  GeminiStartupState* CreateMismatchedStartupState() {
    GeminiStartupState* startup_state = [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::AppSwitcherAISummarization];
    startup_state.isMismatchedAccount = YES;
    return startup_state;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<AuthenticationService> auth_service_;
  id<SnackbarCommands> mock_snackbar_handler_;
  NSMutableArray<SnackbarMessage*>* shown_messages_;
  GeminiContainerSnackbarManager* manager_;
};

// Tests that the mismatched-account identity snackbar is shown when the
// required features are enabled and the user is signed in.
TEST_F(GeminiContainerSnackbarManagerTest,
       TestShowsAccountSnackbarWhenMismatched) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration,
       kAppSwitcherAISummarization, kPageActionMenu},
      {});
  SignInFakeIdentity();

  [manager_ showSnackbarIfAccountMismatchedForStartupState:
                CreateMismatchedStartupState()];

  ASSERT_EQ(1u, shown_messages_.count);
  EXPECT_NSEQ([FakeSystemIdentity fakeIdentity1].userEmail,
              shown_messages_[0].subtitle);
}

// Tests that no snackbar is shown when any precondition is not met or after
// `disconnect`.
TEST_F(GeminiContainerSnackbarManagerTest,
       TestDoesNotShowAccountSnackbarWhenPreconditionsFail) {
  GeminiStartupState* mismatched_state = CreateMismatchedStartupState();
  GeminiStartupState* matching_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::AppSwitcherAISummarization];
  matching_state.isMismatchedAccount = NO;

  // Not signed in.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kAssistantContainer, kIOSGeminiBottomSheetMigration,
         kAppSwitcherAISummarization, kPageActionMenu},
        {});
    [manager_ showSnackbarIfAccountMismatchedForStartupState:mismatched_state];
    EXPECT_EQ(0u, shown_messages_.count);
  }

  SignInFakeIdentity();

  // Bottom sheet migration disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kAppSwitcherAISummarization, kPageActionMenu},
        {kIOSGeminiBottomSheetMigration});
    [manager_ showSnackbarIfAccountMismatchedForStartupState:mismatched_state];
    EXPECT_EQ(0u, shown_messages_.count);
  }

  // App Switcher AI Summarization disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kAssistantContainer, kIOSGeminiBottomSheetMigration},
        {kAppSwitcherAISummarization});
    [manager_ showSnackbarIfAccountMismatchedForStartupState:mismatched_state];
    EXPECT_EQ(0u, shown_messages_.count);
  }

  // Account matches, startup state is nil, or manager is disconnected.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kAssistantContainer, kIOSGeminiBottomSheetMigration,
         kAppSwitcherAISummarization, kPageActionMenu},
        {});
    [manager_ showSnackbarIfAccountMismatchedForStartupState:matching_state];
    [manager_ showSnackbarIfAccountMismatchedForStartupState:nil];
    [manager_ disconnect];
    [manager_ showSnackbarIfAccountMismatchedForStartupState:mismatched_state];
    EXPECT_EQ(0u, shown_messages_.count);
  }
}

}  // namespace
