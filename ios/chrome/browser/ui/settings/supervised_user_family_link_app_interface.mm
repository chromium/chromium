// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/supervised_user_family_link_app_interface.h"

#import <memory>

#import "base/functional/callback.h"
#import "base/ios/block_types.h"
#import "base/memory/ptr_util.h"
#import "base/memory/singleton.h"
#import "base/notreached.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/test_support/browser_state_management.h"
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
#import "ios/chrome/test/app/chrome_test_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace {

supervised_user::BrowserState::Services GetSupervisedUserServicesForProfile(
    ProfileIOS* profile) {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  // SupervisedUserService is not available for the incognito profile.
  CHECK(supervised_user_service);
  return {*supervised_user_service, *profile->GetPrefs(),
          *ios::HostContentSettingsMapFactory::GetForProfile(profile)};
}

// Helper class that holds a instance of the Family Link BrowserState.
// It allows the callers of this class to keep an alive instance
// of a BrowserState for the duration of a test.
class TestFamilyLinkBrowserStateHelper {
 public:
  static TestFamilyLinkBrowserStateHelper* SharedInstance() {
    return base::Singleton<TestFamilyLinkBrowserStateHelper>::get();
  }

  void TearDown() {
    family_link_browser_state_.reset();
    intent_.reset();
  }

  void UpdateWithIntent(
      std::unique_ptr<supervised_user::BrowserState::Intent> intent) {
    intent_ = std::move(intent);
    family_link_browser_state_ =
        std::make_unique<supervised_user::BrowserState>(intent_.get());
  }

  supervised_user::BrowserState* family_link_browser_state() {
    return family_link_browser_state_.get();
  }

  void SeedFamilyLinkBrowserStateWithCompletion(base::OnceClosure completion) {
    // Prepare the services.
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(chrome_test_util::GetOriginalProfile());
    CHECK(profile);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    auto shared_url_loader_factory =
        GetApplicationContext()->GetSharedURLLoaderFactory();
    CHECK(shared_url_loader_factory);

    // The TestFamilyLinkBrowserStateHelper must already be set up by tests.
    CHECK(family_link_browser_state());

    CoreAccountId account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    family_link_browser_state()->SeedWithCompletion(
        *identity_manager, shared_url_loader_factory, account_id.ToString(),
        std::move(completion));
  }

 private:
  std::unique_ptr<supervised_user::BrowserState::Intent> intent_;
  std::unique_ptr<supervised_user::BrowserState> family_link_browser_state_;
};

supervised_user::BrowserState* family_link_browser_state() {
  return TestFamilyLinkBrowserStateHelper::SharedInstance()
      ->family_link_browser_state();
}

}  // namespace

@implementation SupervisedUserFamilyLinkAppInterface : NSObject

+ (BOOL)isBrowserStateSeeded {
  CHECK(family_link_browser_state());
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(chrome_test_util::GetOriginalProfile());
  return family_link_browser_state()->Check(
      GetSupervisedUserServicesForProfile(profile));
}

+ (void)resetFamilyLinkStateWithCompletion:(ProceduralBlock)completion {
  if (family_link_browser_state()) {
    // We should not interrupt a BrowserState that has not completed seeding.
    CHECK([self isBrowserStateSeeded]);
  }
  // Replace the existing BrowserState.
  TestFamilyLinkBrowserStateHelper::SharedInstance()->UpdateWithIntent(
      std::make_unique<supervised_user::BrowserState::ResetIntent>());

  // Seed the BrowserState.
  TestFamilyLinkBrowserStateHelper::SharedInstance()
      ->SeedFamilyLinkBrowserStateWithCompletion(base::BindOnce(completion));
}

+ (void)tearDownTestFamilyLinkBrowserStateHelper {
  TestFamilyLinkBrowserStateHelper::SharedInstance()->TearDown();
}

@end
