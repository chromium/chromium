// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

class SignoutActionSheetCoordinatorTest : public PlatformTest {
 public:
  SignoutActionSheetCoordinatorTest() {
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void SetUp() override {
    PlatformTest::SetUp();

    identity_ = [FakeSystemIdentity fakeIdentity1];
    managed_identity_ = [FakeSystemIdentity fakeManagedIdentity];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    system_identity_manager->AddIdentity(managed_identity_);
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
  }

  void TearDown() override {
    [signout_coordinator_ stop];
    signout_coordinator_ = nil;
    PlatformTest::TearDown();
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  // Sign-out coordinator.
  SignoutActionSheetCoordinator* CreateCoordinator() {
    signout_coordinator_ = [[SignoutActionSheetCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                              rect:view_controller_.view.frame
                              view:view_controller_.view
          forceSnackbarOverToolbar:NO
                        withSource:signin_metrics::ProfileSignout::
                                       kUserClickedSignoutSettings];
    signout_coordinator_.signoutCompletion = ^(BOOL success) {
      completion_callback_.Run(success);
    };
    return signout_coordinator_;
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

 protected:
  // Needed for test profile created by TestProfileIOS().
  base::test::TaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  base::test::ScopedFeatureList scoped_feature_list_;

  SignoutActionSheetCoordinator* signout_coordinator_ = nullptr;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_ = nullptr;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<SystemIdentity> identity_ = nil;
  id<SystemIdentity> managed_identity_ = nil;
  id<SnackbarCommands> snackbar_handler_ =
      OCMStrictProtocolMock(@protocol(SnackbarCommands));
  base::MockRepeatingCallback<void(bool)> completion_callback_;

  raw_ptr<syncer::MockSyncService> sync_service_mock_ = nullptr;
};

// Tests that a signed-in user with Sync enabled will have an action sheet with
// a sign-out title.
// TODO(crbug.com/40066949): Remove this test once ConsentLevel::kSync does not
// exist on iOS anymore.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithSync) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  authentication_service()->GrantSyncConsent(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(true));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_NE(nil, signout_coordinator_.title);
}

// Tests that a signed-in managed user with Sync enabled will have an action
// sheet with a sign-out title.
// TODO(crbug.com/40066949): Remove this test once ConsentLevel::kSync does not
// exist on iOS anymore.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInManagedUserWithSync) {
  authentication_service()->SignIn(
      managed_identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  authentication_service()->GrantSyncConsent(
      managed_identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSync));
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(true));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_NE(nil, signout_coordinator_.title);
}

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldNotShowActionSheetIfNoUnsyncedData) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateCoordinator();
  // Mock returning no unsynced datatype.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(syncer::DataTypeSet)> callback) {
            std::move(callback).Run(syncer::DataTypeSet());
          });
  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  histogram_tester.ExpectTotalCount("Sync.UnsyncedDataOnSignout2", 0u);
  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

TEST_F(SignoutActionSheetCoordinatorTest, ShouldShowActionSheetIfUnsyncedData) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateCoordinator();
  // Mock returning unsynced datatypes.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(syncer::DataTypeSet)> callback) {
            constexpr syncer::DataTypeSet kUnsyncedTypes = {
                syncer::BOOKMARKS, syncer::PREFERENCES};
            std::move(callback).Run(
                base::Intersection(kUnsyncedTypes, requested_types));
          });
  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  histogram_tester.ExpectTotalCount("Sync.UnsyncedDataOnSignout2", 1u);
  histogram_tester.ExpectBucketCount("Sync.UnsyncedDataOnSignout2",
                                     syncer::DataTypeForHistograms::kBookmarks,
                                     1u);
  // Only a few "interesting" data types are recorded. PREFERENCES is not.
  histogram_tester.ExpectBucketCount(
      "Sync.UnsyncedDataOnSignout2",
      syncer::DataTypeForHistograms::kPreferences, 0u);

  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

// Same as ShouldShowActionSheetIfUnsyncedData, but for a managed user.
TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetIfUnsyncedDataForManagedUser) {
  // Sign in with a *managed* account.
  authentication_service()->SignIn(
      managed_identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));

  CreateCoordinator();
  // Mock returning unsynced datatypes, and ensure that this does get called -
  // the action sheet should *not* automatically get shown for a managed user.
  EXPECT_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(syncer::DataTypeSet)> callback) {
            constexpr syncer::DataTypeSet kUnsyncedTypes = {
                syncer::BOOKMARKS, syncer::PREFERENCES};
            std::move(callback).Run(
                base::Intersection(kUnsyncedTypes, requested_types));
          });
  EXPECT_CALL(completion_callback_, Run);

  [signout_coordinator_ start];
}

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetForManagedUserMigratedFromSyncing) {
  // Sign in with a *managed* account.
  authentication_service()->SignIn(
      managed_identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  // Mark the user as "migrated from previously syncing".
  GetPrefs()->SetString(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn,
      base::SysNSStringToUTF8(
          authentication_service()
              ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
              .gaiaID));
  GetPrefs()->SetString(
      prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn,
      base::SysNSStringToUTF8(
          authentication_service()
              ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
              .userEmail));

  CreateCoordinator();
  // There should be no query for unsynced data types: For a managed user who
  // was migrated from the syncing state, the action sheet (asking to user to
  // clear all data) should be shown independently of any unsynced data.
  EXPECT_CALL(*sync_service_mock_, GetTypesWithUnsyncedData).Times(0);
  EXPECT_CALL(completion_callback_, Run);

  [signout_coordinator_ start];
}

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetForManagedUserWithClearDataonSignoutFeature) {
  scoped_feature_list_.InitWithFeatures(
      {kClearDeviceDataOnSignOutForManagedUsers}, {});

  // Sign in with a *managed* account.
  authentication_service()->SignIn(
      managed_identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));

  CreateCoordinator();

  [signout_coordinator_ start];
  ASSERT_NE(nil, signout_coordinator_.title);
}

// TODO(crbug.com/40075765): Add test for recording signout outcome upon warning
// dialog for unsynced data (i.e. for Sync.SignoutWithUnsyncedData).
