// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/supervised_user_family_link_app_interface.h"

#import <memory>

#import "base/functional/callback.h"
#import "base/ios/block_types.h"
#import "base/memory/ptr_util.h"
#import "base/memory/singleton.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/test_support/family_link_settings_state_management.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace {

supervised_user::FamilyLinkSettingsState::Services
GetSupervisedUserServicesForProfile(ProfileIOS* profile) {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  // SupervisedUserService is not available for the incognito profile.
  CHECK(supervised_user_service);
  return {*supervised_user_service, *profile->GetPrefs(),
          *ios::HostContentSettingsMapFactory::GetForProfile(profile)};
}

// Helper class that holds a instance of the Family Link State.
// It allows the callers of this class to keep an alive instance
// of a FamilyLinkSettingsState for the duration of a test.
class TestFamilyLinkSettingsStateHelper {
 public:
  static TestFamilyLinkSettingsStateHelper* SharedInstance() {
    return base::Singleton<TestFamilyLinkSettingsStateHelper>::get();
  }

  void TearDown() { family_link_settings_state_.reset(); }

  bool IsFamilyLinkSettingsStateSeeded() {
    CHECK(family_link_settings_state_);
    ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
    return family_link_settings_state_->Check(
        GetSupervisedUserServicesForProfile(profile));
  }

  void UpdateIntentAndSeedFamilyLinkSettingsState(
      std::unique_ptr<supervised_user::FamilyLinkSettingsState::Intent>
          intent) {
    // We should only update the FamilyLinkSettingsState if it is not yet
    // created, or if it has been completely seeded.
    CHECK(family_link_settings_state_ == nullptr ||
          IsFamilyLinkSettingsStateSeeded());
    family_link_settings_state_ =
        std::make_unique<supervised_user::FamilyLinkSettingsState>(
            std::move(intent));
    SeedFamilyLinkSettingsState();
  }

 private:
  void SeedFamilyLinkSettingsState() {
    // Prepare the services.
    ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
    CHECK(profile);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    auto shared_url_loader_factory =
        GetApplicationContext()->GetSharedURLLoaderFactory();
    CHECK(shared_url_loader_factory);

    // The TestFamilyLinkSettingsStateHelper must already be set up by tests.
    CHECK(family_link_settings_state_);

    CoreAccountId account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    family_link_settings_state_->StartSeeding(
        *identity_manager, shared_url_loader_factory, account_id.ToString());
  }

  std::unique_ptr<supervised_user::FamilyLinkSettingsState>
      family_link_settings_state_;
};

}  // namespace

@implementation SupervisedUserFamilyLinkAppInterface : NSObject

+ (BOOL)isFamilyLinkSettingsStateSeeded {
  return TestFamilyLinkSettingsStateHelper::SharedInstance()
      ->IsFamilyLinkSettingsStateSeeded();
}

+ (void)seedDefaultFamilyLinkSettings {
  TestFamilyLinkSettingsStateHelper::SharedInstance()
      ->UpdateIntentAndSeedFamilyLinkSettingsState(
          std::make_unique<
              supervised_user::FamilyLinkSettingsState::ResetIntent>());
}

+ (void)seedSafeSitesFiltering {
  TestFamilyLinkSettingsStateHelper::SharedInstance()
      ->UpdateIntentAndSeedFamilyLinkSettingsState(
          std::make_unique<supervised_user::FamilyLinkSettingsState::
                               DefineManualSiteListIntent>());
}

+ (void)seedAllowSite:(NSString*)url {
  TestFamilyLinkSettingsStateHelper::SharedInstance()
      ->UpdateIntentAndSeedFamilyLinkSettingsState(
          std::make_unique<supervised_user::FamilyLinkSettingsState::
                               DefineManualSiteListIntent>(
              supervised_user::FamilyLinkSettingsState::
                  DefineManualSiteListIntent::AllowUrl(
                      GURL(base::SysNSStringToUTF8(url)))));
}

+ (void)seedBlockSite:(NSString*)url {
  TestFamilyLinkSettingsStateHelper::SharedInstance()
      ->UpdateIntentAndSeedFamilyLinkSettingsState(
          std::make_unique<supervised_user::FamilyLinkSettingsState::
                               DefineManualSiteListIntent>(
              supervised_user::FamilyLinkSettingsState::
                  DefineManualSiteListIntent::BlockUrl(
                      GURL(base::SysNSStringToUTF8(url)))));
}

+ (void)triggerSyncServiceRefresh {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  SyncServiceFactory::GetForProfile(profile)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown,
      syncer::DataTypeSet::All());
}

+ (void)tearDownTestFamilyLinkSettingsStateHelper {
  TestFamilyLinkSettingsStateHelper::SharedInstance()->TearDown();
}

@end
