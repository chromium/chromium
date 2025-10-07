// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/supervised_user_settings_app_interface.h"

#import "base/memory/ptr_util.h"
#import "base/memory/singleton.h"
#import "base/version_info/channel.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/permission_request_creator.h"
#import "components/supervised_user/core/browser/permission_request_creator_mock.h"
#import "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_test_environment.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

namespace {

class StaticUrlCheckerClient : public safe_search_api::URLCheckerClient {
 public:
  explicit StaticUrlCheckerClient(
      safe_search_api::ClientClassification response)
      : response_(response) {}
  void CheckURL(const GURL& url, ClientCheckCallback callback) override {
    std::move(callback).Run(url, response_);
  }

 private:
  safe_search_api::ClientClassification response_;
};

void setUrlFilteringForUrl(const GURL& url, bool isAllowed) {
  supervised_user::SupervisedUserTestEnvironment::SetManualFilterForHost(
      url.GetHost(), isAllowed,
      *SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile()));
}

bool isShowingInterstitialForState(web::WebState* web_state) {
  CHECK(web_state);
  if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
    // If kCreateTabHelperOnlyForRealizedWebStates feature is enabled, then
    // the tab helpers are not created for unrealized WebStates. If the tab
    // helpers are not created, they cannot be presenting an interstitial,
    // so return early in that case.
    if (!web_state->IsRealized()) {
      return false;
    }
  }

  auto* blocking_tab_helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state);

  // The tab helper must have been created for the WebState at this point.
  CHECK(blocking_tab_helper);

  security_interstitials::IOSSecurityInterstitialPage* blocking_page =
      blocking_tab_helper->GetCurrentBlockingPage();
  return blocking_page && blocking_page->GetInterstitialType() ==
                              kSupervisedUserInterstitialType;
}

}  // namespace

@implementation SupervisedUserSettingsAppInterface : NSObject

+ (void)setSupervisedUserURLFilterBehavior:
    (supervised_user::FilteringBehavior)behavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(static_cast<int>(behavior)));
  if (behavior == supervised_user::FilteringBehavior::kAllow) {
    settings_service->SetLocalSetting(supervised_user::kSafeSitesEnabled,
                                      base::Value(true));
  }
}

+ (void)resetSupervisedUserURLFilterBehavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior);
}

+ (void)resetManualUrlFiltering {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts);
}

+ (void)setFakePermissionCreator {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  CHECK(settings_service);
  std::unique_ptr<supervised_user::PermissionRequestCreator> creator =
      std::make_unique<supervised_user::PermissionRequestCreatorMock>(
          *settings_service);
  supervised_user::PermissionRequestCreatorMock* mocked_creator =
      static_cast<supervised_user::PermissionRequestCreatorMock*>(
          creator.get());
  mocked_creator->SetEnabled();

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  CHECK(service);
  service->remote_web_approvals_manager().ClearApprovalRequestsCreators();
  service->remote_web_approvals_manager().AddApprovalRequestCreator(
      std::move(creator));
}

+ (void)approveWebsiteDomain:(NSURL*)url {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  settings_service->RecordLocalWebsiteApproval(
      net::GURLWithNSURL(url).GetHost());
}

+ (void)setFilteringToAllowAllSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::FilteringBehavior::
                                               kAllow];
}

+ (void)setFilteringToAllowApprovedSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::FilteringBehavior::
                                               kBlock];
}

+ (void)addWebsiteToAllowList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), true);
}

+ (void)addWebsiteToBlockList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), false);
}

+ (void)setDefaultClassifyURLNavigationIsAllowed:(BOOL)is_allowed {
  SupervisedUserServiceFactory::GetInstance()
      ->GetForProfile(chrome_test_util::GetOriginalProfile())
      ->GetURLFilter()
      ->SetURLCheckerClientForTesting(std::make_unique<StaticUrlCheckerClient>(
          is_allowed ? safe_search_api::ClientClassification::kAllowed
                     : safe_search_api::ClientClassification::kRestricted));
}

+ (NSInteger)countSupervisedUserIntersitialsForExistingWebStates {
  int count = 0;
  int tab_count = chrome_test_util::GetMainTabCount();
  for (int i = 0; i < tab_count; i++) {
    if (isShowingInterstitialForState(
            chrome_test_util::GetWebStateAtIndexInCurrentMode(i))) {
      count++;
    }
  }
  return count;
}

@end
