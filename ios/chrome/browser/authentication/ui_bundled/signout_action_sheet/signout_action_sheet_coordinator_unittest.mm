// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/test/mock_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
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
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage()
        ->SetPersonalProfileName(profile_->GetProfileName());

    identity_ = [FakeSystemIdentity fakeIdentity1];
    managed_identity_ = [FakeSystemIdentity fakeManagedIdentity];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    system_identity_manager->AddIdentity(managed_identity_);

    ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
    profile_state.profile = profile_.get();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state;

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    Browser* browser =
        scene_state_.browserProviderInterface.currentBrowserProvider.browser;
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
    [browser->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    // Ensure the AuthenticationService is created: It does some first-time
    // setup on construction, and it's confusing if that happens implicitly on
    // the first access, potentially in the middle of a test.
    authentication_service();
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)scene_handler_);
    @autoreleasepool {
      [signout_coordinator_ stop];
      [scene_state_ shutdown];
      signout_coordinator_ = nil;
      scene_state_ = nil;
    }
    PlatformTest::TearDown();
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  // Sign-out coordinator.
  SignoutActionSheetCoordinator* CreateCoordinator(
      BOOL show_undo_button,
      signin_metrics::ProfileSignout source) {
    Browser* browser =
        scene_state_.browserProviderInterface.currentBrowserProvider.browser;
    signout_coordinator_ = [[SignoutActionSheetCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser
                              rect:view_controller_.view.frame
                              view:view_controller_.view
          forceSnackbarOverToolbar:NO
                    showUndoButton:show_undo_button
                        withSource:source
                        completion:^(BOOL success, SceneState* scene_state) {
                          [signout_coordinator_ stop];
                          signout_coordinator_ = nil;
                          completion_callback_.Run(success);
                        }];
    return signout_coordinator_;
  }

  SignoutActionSheetCoordinator* CreateCoordinator() {
    return CreateCoordinator(
        /*show_undo_button=*/NO,
        signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  void SignInManagedIdentity() {
    // These tests only apply when a managed account is signed in to the
    // personal profile (which, in prod, can only happen if the account was
    // already signed in before kSeparateProfilesForManagedAccounts was
    // enabled). This situation is tricky to replicate in a unit test; it's done
    // here by first converting the (single) test profile to a managed profile,
    // then marking it as the personal profile again.
    // TODO(crbug.com/407498240): Remove the affected tests once all users are
    // migrated to kSeparateProfilesForManagedAccounts.
    GetApplicationContext()
        ->GetAccountProfileMapper()
        ->MakePersonalProfileManagedWithGaiaID(managed_identity_.gaiaId);

    authentication_service()->SignIn(managed_identity_,
                                     signin_metrics::AccessPoint::kStartPage);

    // To set the personal profile.
    GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage()
        ->SetPersonalProfileName(profile_->GetProfileName());

    ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged());
  }

 protected:
  // Needed for test profile created by TestProfileIOS().
  web::WebTaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  SignoutActionSheetCoordinator* signout_coordinator_ = nullptr;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_ = nullptr;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  id<SystemIdentity> identity_ = nil;
  id<SystemIdentity> managed_identity_ = nil;
  id<SnackbarCommands> snackbar_handler_ =
      OCMStrictProtocolMock(@protocol(SnackbarCommands));
  id<SceneCommands> scene_handler_ =
      OCMStrictProtocolMock(@protocol(SceneCommands));
  base::MockRepeatingCallback<void(bool)> completion_callback_;

  raw_ptr<syncer::MockSyncService> sync_service_mock_ = nullptr;
};

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldNotShowActionSheetIfNoUnsyncedData) {
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

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
                                   signin_metrics::AccessPoint::kStartPage);

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

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksLimitExceededOnSignoutPrompt", false, 1u);

  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetForManagedUserMigratedFromSyncing) {
  // Sign in with a *managed* account.
  SignInManagedIdentity();

  // Mark the user as "migrated from previously syncing".
  GetPrefs()->SetString(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn,
      authentication_service()->GetPrimaryIdentity().gaiaId.ToString());
  GetPrefs()->SetString(
      prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn,
      base::SysNSStringToUTF8(
          authentication_service()->GetPrimaryIdentity().userEmail));

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

TEST_F(SignoutActionSheetCoordinatorTest,
       ShouldShowActionSheetIfBookmarksLimitExceeded) {
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

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

  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(testing::Return(
          syncer::SyncService::UserActionableError::kBookmarksLimitExceeded));

  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  // The action sheet should be shown.
  ASSERT_NE(nil, signout_coordinator_.message);

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksLimitExceededOnSignoutPrompt", true, 1u);
}

// Tests that the snackbar message shown after user-initiated sign-out includes
// an undo action, and that triggering the action records the user action.
TEST_F(SignoutActionSheetCoordinatorTest, SignoutSnackbarMessageHasUndoAction) {
  base::test::ScopedFeatureList feature_list(kIdentityAwareness);
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

  CreateCoordinator(
      /*show_undo_button=*/YES,
      signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu);

  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(
                 absl::flat_hash_map<syncer::DataType, size_t>)> callback) {
            std::move(callback).Run({});
          });

  __block SnackbarMessage* captured_message = nil;
  bool message_captured = false;
  bool* message_captured_ptr = &message_captured;
  OCMExpect([snackbar_handler_
      showSnackbarMessage:[OCMArg
                              checkWithBlock:^BOOL(SnackbarMessage* message) {
                                captured_message = message;
                                *message_captured_ptr = true;
                                return YES;
                              }]
             bottomOffset:0]);

  EXPECT_CALL(completion_callback_, Run);

  [signout_coordinator_ start];
  EXPECT_TRUE(base::test::RunUntil(
      [message_captured_ptr]() { return *message_captured_ptr; }));

  EXPECT_OCMOCK_VERIFY((id)snackbar_handler_);
  ASSERT_NE(nil, captured_message);
  ASSERT_NE(nil, captured_message.action);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SIGNIN_SNACKBAR_UNDO),
              captured_message.action.title);

  base::UserActionTester user_action_tester;
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity());

  OCMExpect([scene_handler_ showUndoSignoutFromSnackbarForIdentity:identity_]);

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Mobile.Signout.SnackbarUndoTapped"));
  captured_message.action.handler();
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Mobile.Signout.SnackbarUndoTapped"));
}

// Tests that the snackbar message shown after non-user-initiated sign-out
// does not include an undo action.
TEST_F(SignoutActionSheetCoordinatorTest,
       SignoutSnackbarMessageHasNoUndoActionWhenNotUserInitiated) {
  base::test::ScopedFeatureList feature_list(kIdentityAwareness);
  authentication_service()->SignIn(identity_,
                                   signin_metrics::AccessPoint::kStartPage);

  CreateCoordinator(
      /*show_undo_button=*/NO,
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching);

  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](syncer::DataTypeSet requested_types,
             base::OnceCallback<void(
                 absl::flat_hash_map<syncer::DataType, size_t>)> callback) {
            std::move(callback).Run({});
          });

  __block SnackbarMessage* captured_message = nil;
  bool message_captured = false;
  bool* message_captured_ptr = &message_captured;
  OCMExpect([snackbar_handler_
      showSnackbarMessage:[OCMArg
                              checkWithBlock:^BOOL(SnackbarMessage* message) {
                                captured_message = message;
                                *message_captured_ptr = true;
                                return YES;
                              }]
             bottomOffset:0]);

  [signout_coordinator_ start];
  EXPECT_TRUE(base::test::RunUntil(
      [message_captured_ptr]() { return *message_captured_ptr; }));

  EXPECT_OCMOCK_VERIFY((id)snackbar_handler_);
  ASSERT_NE(nil, captured_message);
  EXPECT_EQ(nil, captured_message.action);
}

// TODO(crbug.com/40075765): Add test for recording signout outcome upon warning
// dialog for unsynced data (i.e. for Sync.SignoutWithUnsyncedData).
