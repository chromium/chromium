// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/command_line.h"
#import "base/logging.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/sharing/fake_recipients_fetcher.h"
#import "components/password_manager/ios/fake_bulk_leak_check_service.h"
#import "components/plus_addresses/fake_plus_address_service.h"
#import "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/internal/tab_group_sync_coordinator.h"
#import "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"
#import "components/saved_tab_groups/public/features.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/drive/model/test_drive_service.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/policy/model/test_platform_policy_provider.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"

namespace tests_hook {

class IOSFakeTabGroupSyncService : public tab_groups::FakeTabGroupSyncService {
 public:
  void SetTabGroupSyncDelegate(
      std::unique_ptr<tab_groups::TabGroupSyncDelegate> delegate) override;

  void SetCoordinator(
      std::unique_ptr<tab_groups::TabGroupSyncCoordinator> coordinator);

 private:
  // The UI coordinator to apply changes between local tab groups and the
  // TabGroupSyncService.
  std::unique_ptr<tab_groups::TabGroupSyncCoordinator> coordinator_;
};

void IOSFakeTabGroupSyncService::SetTabGroupSyncDelegate(
    std::unique_ptr<tab_groups::TabGroupSyncDelegate> delegate) {
  auto coordinator = std::make_unique<tab_groups::TabGroupSyncCoordinatorImpl>(
      std::move(delegate), this);
  SetCoordinator(std::move(coordinator));
}

void IOSFakeTabGroupSyncService::SetCoordinator(
    std::unique_ptr<tab_groups::TabGroupSyncCoordinator> coordinator) {
  CHECK(!coordinator_);
  coordinator_ = std::move(coordinator);
  AddObserver(coordinator_.get());
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

bool DisablePromoManagerFullScreenPromos() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePromoManagerFullscreenPromos);
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

bool DisableUpgradeSigninPromo() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUpgradeSigninPromo);
}

bool DisableUpdateService() {
  return true;
}

bool DelayAppLaunchPromos() {
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

    identities =
        [FakeSystemIdentity identitiesFromBase64String:command_line_value];
  }

  auto system_identity_manager =
      std::make_unique<FakeSystemIdentityManager>(identities);

  // Add a fake identity if asked to start the app in signed-in state but
  // no identity was passed via the kAddFakeIdentitiesAtStartup parameter.
  if (identities.count == 0 &&
      command_line->HasSwitch(test_switches::kSignInAtStartup)) {
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  }

  return system_identity_manager;
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

  if (!IsTabGroupSyncEnabled() ||
      !command_line->HasSwitch(test_switches::kEnableFakeTabGroupSyncService)) {
    return nullptr;
  }
  auto sync_service = std::make_unique<IOSFakeTabGroupSyncService>();

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

  std::unique_ptr<tab_groups::TabGroupLocalUpdateObserver>
      local_update_observer =
          std::make_unique<tab_groups::TabGroupLocalUpdateObserver>(
              browser_list, sync_service.get());

  std::unique_ptr<tab_groups::IOSTabGroupSyncDelegate> delegate =
      std::make_unique<tab_groups::IOSTabGroupSyncDelegate>(
          browser_list, sync_service.get(), std::move(local_update_observer));

  sync_service->SetCoordinator(
      std::make_unique<tab_groups::TabGroupSyncCoordinatorImpl>(
          std::move(delegate), sync_service.get()));

  return sync_service;
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
  // No-op for Earl Grey.
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

base::TimeDelta GetOverriddenSnackbarDuration() {
  // Increase the snackbar duration for EGTests for test to catch it more
  // easily.
  return base::Seconds(MDCSnackbarMessageDurationMax);
}

std::unique_ptr<drive::DriveService> GetOverriddenDriveService() {
  return std::make_unique<drive::TestDriveService>();
}

std::optional<std::string> FETDemoModeOverride() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          test_switches::kEnableIPH)) {
    // The FET Demo Mode tracker uses the returned string here as the feature
    // name to enable. Using a feature name that doesn't exist will disable all
    // IPH in tests. This is the desired behavior for EG tests if no specific
    // feature is enabled.
    return "disable_all";
  }
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      test_switches::kEnableIPH);
}

}  // namespace tests_hook
