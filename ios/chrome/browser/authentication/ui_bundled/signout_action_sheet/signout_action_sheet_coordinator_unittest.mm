// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/test/mock_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
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
#import "ios/web/public/test/web_task_environment.h"
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

    TestProfileIOS::Builder builder;
    builder.SetName(GetApplicationContext()
                        ->GetProfileManager()
                        ->GetProfileAttributesStorage()
                        ->GetPersonalProfileName());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    identity_ = [FakeSystemIdentity fakeIdentity1];
    managed_identity_ = [FakeSystemIdentity fakeManagedIdentity];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    system_identity_manager->AddIdentity(managed_identity_);

    AppState* app_state = [[AppState alloc] initWithStartupInformation:nil];
    SceneState* scene_state = [[SceneState alloc] initWithAppState:app_state];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state);

    stub_browser_interface_provider_ =
        [[StubBrowserProviderInterface alloc] init];
    stub_browser_interface_provider_.mainBrowserProvider.browser =
        browser_.get();
    scene_state_mock_ = OCMPartialMock(scene_state);
    OCMStub([scene_state_mock_ browserProviderInterface])
        .andReturn(stub_browser_interface_provider_);

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    // Ensure the AuthenticationService is created: It does some first-time
    // setup on construction, and it's confusing if that happens implicitly on
    // the first access, potentially in the middle of a test.
    authentication_service();
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)scene_state_mock_);
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
    constexpr signin_metrics::ProfileSignout metricSignOut =
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

    signout_coordinator_ = [[SignoutActionSheetCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                              rect:view_controller_.view.frame
                              view:view_controller_.view
          forceSnackbarOverToolbar:NO
                        withSource:metricSignOut
                        completion:^(BOOL success, SceneState* scene_state) {
                          signout_coordinator_ = nil;
                          completion_callback_.Run(success);
                        }];
    return signout_coordinator_;
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  void SignInManagedIdentity() {
    if (!AreSeparateProfilesForManagedAccountsEnabled()) {
      authentication_service()->SignIn(managed_identity_,
                                       signin_metrics::AccessPoint::kUnknown);
    } else {
      // With kSeparateProfilesForManagedAccounts, these tests only apply when a
      // managed account is signed in to the personal profile (which, in prod,
      // can only happen if the account was already signed in before
      // kSeparateProfilesForManagedAccounts was enabled). This situation is
      // tricky to replicate in a unit test; it's done here by first converting
      // the (single) test profile to a managed profile, then marking it as the
      // personal profile again.
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->MakePersonalProfileManagedWithGaiaID(managed_identity_.gaiaId);

      authentication_service()->SignIn(managed_identity_,
                                       signin_metrics::AccessPoint::kUnknown);

      GetApplicationContext()
          ->GetProfileManager()
          ->GetProfileAttributesStorage()
          ->SetPersonalProfileName(profile_->GetProfileName());
    }
    ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
        signin::ConsentLevel::kSignin));
  }

 protected:
  // Needed for test profile created by TestProfileIOS().
  web::WebTaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  base::test::ScopedFeatureList scoped_feature_list_;

  SignoutActionSheetCoordinator* signout_coordinator_ = nullptr;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_ = nullptr;
  // Partial mock for stubbing scene_state's methods
  SceneState* scene_state_mock_;
  StubBrowserProviderInterface* stub_browser_interface_provider_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<TestProfileIOS, DanglingUntriaged> profile_;
  TestProfileManagerIOS profile_manager_;
  id<SystemIdentity> identity_ = nil;
  id<SystemIdentity> managed_identity_ = nil;
  id<SnackbarCommands> snackbar_handler_ =
      OCMStrictProtocolMock(@protocol(SnackbarCommands));
  base::MockRepeatingCallback<void(bool)> completion_callback_;

  raw_ptr<syncer::MockSyncService, DanglingUntriaged> sync_service_mock_ =
      nullptr;
};

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldNotShowActionSheetIfNoUnsyncedData) {
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kUnknown);

  CreateCoordinator();
  // Mock returning no unsynced datatype.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(
                 absl::flat_hash_map<syncer::DataType, size_t>)> callback) {
            std::move(callback).Run(
                absl::flat_hash_map<syncer::DataType, size_t>());
          });
  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  histogram_tester.ExpectTotalCount("Sync.UnsyncedDataOnSignout2", 0u);
  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

TEST_F(SignoutActionSheetCoordinatorTest, ShouldShowActionSheetIfUnsyncedData) {
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kUnknown);

  CreateCoordinator();
  // Mock returning unsynced datatypes.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(
                 absl::flat_hash_map<syncer::DataType, size_t>)> callback) {
            constexpr syncer::DataTypeSet kUnsyncedTypes = {
                syncer::BOOKMARKS, syncer::PREFERENCES};
            syncer::DataTypeSet returned_types =
                base::Intersection(kUnsyncedTypes, requested_types);
            absl::flat_hash_map<syncer::DataType, size_t> type_counts;
            for (auto type : returned_types) {
              type_counts[type] = 1u;
            }
            std::move(callback).Run(std::move(type_counts));
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

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetForManagedUserMigratedFromSyncing) {
  // Sign in with a *managed* account.
  SignInManagedIdentity();

  // Mark the user as "migrated from previously syncing".
  GetPrefs()->SetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn,
                        authentication_service()
                            ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
                            .gaiaId.ToString());
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
  // Sign in with a *managed* account.
  SignInManagedIdentity();

  CreateCoordinator();

  [signout_coordinator_ start];
  ASSERT_NE(nil, signout_coordinator_.title);
}

// TODO(crbug.com/40075765): Add test for recording signout outcome upon warning
// dialog for unsynced data (i.e. for Sync.SignoutWithUnsyncedData).
