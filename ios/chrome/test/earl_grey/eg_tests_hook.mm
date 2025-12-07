// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#import "ios/chrome/app/tests_hook.h"
// clang-format on

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/command_line.h"
#import "base/containers/contains.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/allow_check_is_test_for_testing.h"
#import "base/time/time.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/commerce/core/shopping_service.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/test_support/mock_preview_server_proxy.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/password_manager/core/browser/sharing/fake_recipients_fetcher.h"
#import "components/password_manager/ios/fake_bulk_leak_check_service.h"
#import "components/plus_addresses/core/browser/fake_plus_address_service.h"
#import "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/internal/saved_tab_group_model.h"
#import "components/saved_tab_groups/internal/tab_group_sync_coordinator.h"
#import "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"
#import "components/saved_tab_groups/internal/tab_group_sync_service_test_utils.h"
#import "components/saved_tab_groups/public/features.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/drive/model/test_drive_service.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/policy/model/test_platform_policy_provider.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Loads a very simple UILabel with a teapot emoji in it as the main
// UI for the given window.
void LoadMinimalAppUIInWindow(UIWindow* window) {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label =
      [[UILabel alloc] initWithFrame:window.windowScene.screen.bounds];
  label.text = @"🫖";
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = [UIColor whiteColor];
  label.backgroundColor = [UIColor darkGrayColor];
  label.font = [UIFont boldSystemFontOfSize:80];
  viewController.view = label;
  window.rootViewController = viewController;
  [window addSubview:viewController.view];
  [window makeKeyAndVisible];
}

}  // namespace

namespace tests_hook {

bool DisableGeminiEligibilityCheck() {
  return true;
}

bool DisableAppGroupAccess() {
  return true;
}

bool DisableClientSideFieldTrials() {
  return true;
}

bool DisableContentSuggestions() {
  return true;
}

bool DisableDiscoverFeed() {
  // Performance tests may disable the discover feed by setting the
  // DISABLE_DISCOVER_FEED environment variable. Possible values
  // the variable may be set to are described in the apple documentation for
  // boolValue:
  // https://developer.apple.com/documentation/foundation/nsstring/1409420-boolvalue
  if ([[NSProcessInfo.processInfo.environment
          objectForKey:@"DISABLE_DISCOVER_FEED"] boolValue]) {
    return true;
  }
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDiscoverFeed);
}

bool DisableDefaultFirstRun() {
  return true;
}

bool DisableDefaultSearchEngineChoice() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisablePromoManagerDisplayingPromo() {
  // In EG tests, all promos are disabled unless explicitly activated by
  // `kEnableIPH`.
  return false;
}

std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  // Do not fake account tracking and authentication services if the user has
  // requested a real identity manager.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          test_switches::kForceRealSystemIdentityManager)) {
    return nullptr;
  }
  std::unique_ptr<FakeProfileOAuth2TokenService> token_service =
      std::make_unique<FakeProfileOAuth2TokenService>(user_prefs,
                                                      std::move(delegate));
  // Posts auth token requests immediately on request instead of waiting for an
  // explicit `IssueTokenForScope` call.
  token_service->set_auto_post_fetch_response_on_message_loop(true);
  return token_service;
}

bool DisableFullscreenSigninPromo() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFullscreenSigninPromo);
}

bool DisableUpdateService() {
  return true;
}

bool DelayAppLaunchPromos() {
  return true;
}

bool NeverPurgeDiscardedSessionsData() {
  return true;
}

bool LoadMinimalAppUI() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          test_switches::kLoadMinimalAppUI)) {
    return false;
  }
  static bool minimal_ui_loaded = false;
  if (!minimal_ui_loaded) {
    NSSet<UIScene*>* scenes = UIApplication.sharedApplication.connectedScenes;
    for (UIScene* scene in scenes) {
      UIWindowScene* window_scene =
          base::apple::ObjCCastStrict<UIWindowScene>(scene);
      for (UIWindow* window in window_scene.windows) {
        if (window.canBecomeKeyWindow) {
          LoadMinimalAppUIInWindow(window);
          minimal_ui_loaded = true;
          return true;
        };
      }
    }
  };
  return true;
}

policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "com.apple.configuration.managed")) {
    DVLOG(1) << "Policy data present in NSUserDefaults, not installing test "
                "platform provider";
    return nullptr;
  }
  return GetTestPlatformPolicyProvider();
}

bool SimulatePostDeviceRestore() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      test_switches::kSimulatePostDeviceRestore);
}

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(test_switches::kForceRealSystemIdentityManager)) {
    // By returning nullptr, we force ApplicationContext to use the provider to
    // create the SystemIdentityManager.
    return nullptr;
  }

  NSArray<id<SystemIdentity>>* identities = @[];
  if (command_line->HasSwitch(test_switches::kAddFakeIdentitiesAtStartup)) {
    const std::string command_line_value = command_line->GetSwitchValueASCII(
        test_switches::kAddFakeIdentitiesAtStartup);

    if (command_line_value.empty()) {
      // If no identities were passed via parameter, add a single fake identity.
      identities = [NSArray arrayWithObject:[FakeSystemIdentity fakeIdentity1]];
    } else {
      identities =
          [FakeSystemIdentity identitiesFromBase64String:command_line_value];
    }
  }

  return std::make_unique<FakeSystemIdentityManager>(identities);
}

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(test_switches::kForceRealSystemIdentityManager)) {
    // By returning nullptr, we force ApplicationContext to use the provider to
    // create the FakeTrustedVaultClientBackend.
    return nullptr;
  }
  return std::make_unique<FakeTrustedVaultClientBackend>();
}

std::unique_ptr<tab_groups::TabGroupSyncService> CreateTabGroupSyncService(
    ProfileIOS* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(test_switches::kEnableFakeTabGroupSyncService)) {
    return nullptr;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  auto model = std::make_unique<tab_groups::SavedTabGroupModel>();
  auto* opt_guide = OptimizationGuideServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  std::unique_ptr<tab_groups::TabGroupSyncService> sync_service =
      tab_groups::test::CreateTabGroupSyncService(
          std::move(model), DataTypeStoreServiceFactory::GetForProfile(profile),
          profile->GetPrefs(), device_info_tracker, opt_guide,
          identity_manager);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

  std::unique_ptr<tab_groups::TabGroupLocalUpdateObserver>
      local_update_observer =
          std::make_unique<tab_groups::TabGroupLocalUpdateObserver>(
              browser_list, sync_service.get(),
              SessionRestorationServiceFactory::GetForProfile(profile));

  std::unique_ptr<tab_groups::IOSTabGroupSyncDelegate> delegate =
      std::make_unique<tab_groups::IOSTabGroupSyncDelegate>(
          browser_list, sync_service.get(), std::move(local_update_observer));
  sync_service->SetTabGroupSyncDelegate(std::move(delegate));

  return sync_service;
}

std::unique_ptr<commerce::ShoppingService> CreateShoppingService(
    ProfileIOS* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(test_switches::kMockShoppingService)) {
    return nullptr;
  }

  auto service =
      std::make_unique<testing::NiceMock<commerce::MockShoppingService>>();

  const std::vector<std::string> args = base::SplitString(
      command_line->GetSwitchValueASCII(test_switches::kMockShoppingService),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& value : args) {
    if (value == "is-eligible") {
      service->SetIsShoppingListEligible(true);
      continue;
    }

    if (value == "has-empty-price-tracked-bookmarks-results") {
      service->SetGetAllPriceTrackedBookmarksCallbackValue({});
      continue;
    }
    if (value == "has-empty-subscriptions-results") {
      service->SetGetAllSubscriptionsCallbackValue({});
      continue;
    }
  }

  return service;
}

void DataSharingServiceHooks(
    data_sharing::DataSharingService* data_sharing_service) {
  auto preview_server_proxy =
      std::make_unique<data_sharing::MockPreviewServerProxy>();
  data_sharing_service->SetPreviewServerProxyForTesting(
      std::move(preview_server_proxy));
}

std::unique_ptr<ShareKitService> CreateShareKitService(
    data_sharing::DataSharingService* data_sharing_service,
    collaboration::CollaborationService* collaboration_service,
    tab_groups::TabGroupSyncService* sync_service,
    TabGroupService* tab_group_service) {
  return std::make_unique<TestShareKitService>(data_sharing_service,
                                               collaboration_service,
                                               sync_service, tab_group_service);
}

std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService() {
  return std::make_unique<password_manager::FakeBulkLeakCheckService>();
}

std::unique_ptr<plus_addresses::PlusAddressService>
GetOverriddenPlusAddressService() {
  return std::make_unique<plus_addresses::FakePlusAddressService>();
}

std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  password_manager::FetchFamilyMembersRequestStatus status =
      password_manager::FetchFamilyMembersRequestStatus::kUnknown;
  if (command_line->HasSwitch(test_switches::kFamilyStatus)) {
    std::string command_line_value =
        command_line->GetSwitchValueASCII(test_switches::kFamilyStatus);
    int status_value = 0;
    if (base::StringToInt(command_line_value, &status_value)) {
      status = static_cast<password_manager::FetchFamilyMembersRequestStatus>(
          status_value);
    }
  }

  return std::make_unique<password_manager::FakeRecipientsFetcher>(status);
}

void SetUpTestsIfPresent() {
  base::test::AllowCheckIsTestForTesting();
}

void RunTestsIfPresent() {
  // No-op for Earl Grey.
}

void SignalAppLaunched() {
  // No-op for Earl Grey.
}

base::TimeDelta PasswordCheckMinimumDuration() {
  // No delays for eg tests.
  return base::Seconds(0);
}

std::unique_ptr<drive::DriveService> GetOverriddenDriveService() {
  return std::make_unique<drive::TestDriveService>();
}

feature_engagement::FeatureActivation FETDemoModeOverride() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          test_switches::kEnableIPH)) {
    return feature_engagement::FeatureActivation::AllDisabled();
  }
  return feature_engagement::FeatureActivation(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          test_switches::kEnableIPH));
}

void DeleteFilesRecursively(NSString* directoryPath) {
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray* contents = [fileManager contentsOfDirectoryAtPath:directoryPath
                                                       error:&error];
  for (NSString* itemName in contents) {
    NSString* itemPath =
        [directoryPath stringByAppendingPathComponent:itemName];
    BOOL isDirectory;
    if ([fileManager fileExistsAtPath:itemPath isDirectory:&isDirectory]) {
      if (isDirectory) {
        DeleteFilesRecursively(itemPath);
      } else {
        // Deleting files in /Library/Preferences seems to break
        // NSUserDefaults syncing. Just ignore the directory completely.
        if ([itemPath containsString:@"/Library/Preferences/"]) {
          continue;
        }
        if ([itemPath containsString:@"Saved Application State"]) {
          continue;
        }
        if (![fileManager removeItemAtPath:itemPath error:&error]) {
          NSLog(@"Error deleting file: %@", error.localizedDescription);
        }
      }
    }
  }
}

void WipeProfileIfRequested(base::span<const char* const> args) {
  static constexpr std::string_view kWipeArg = "-EGTestWipeProfile";
  if (!base::Contains(args, kWipeArg)) {
    return;
  }

  DeleteFilesRecursively(
      [NSHomeDirectory() stringByAppendingPathComponent:@"Library"]);

  // Reset NSUserDefaults.
  [[NSUserDefaults standardUserDefaults]
      setPersistentDomain:[NSDictionary dictionary]
                  forName:[[NSBundle mainBundle] bundleIdentifier]];
  [[NSUserDefaults standardUserDefaults] synchronize];
}

base::TimeDelta
GetOverriddenDelayForRequestingTurningOnCredentialProviderExtension() {
  return base::Seconds(2);
}

base::TimeDelta GetSnackbarMessageDuration() {
  // Makes the snackbar duration longer for EGTests to make sure there is time
  // detect it, and avoid flakiness.
  return base::Seconds(30);
}

UIImage* GetPHPickerViewControllerImage() {
  return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(1000, 1000), UIColor.greenColor);
}

}  // namespace tests_hook
