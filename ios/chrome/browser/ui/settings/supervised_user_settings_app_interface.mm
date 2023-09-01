// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/supervised_user_settings_app_interface.h"

#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/permission_request_creator.h"
#import "components/supervised_user/core/browser/permission_request_creator_mock.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/supervised_user/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"

namespace {
void setUrlFilteringForUrl(const GURL& url, bool isAllowed) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  const base::Value::Dict& local_settings =
      settings_service->LocalSettingsForTest();
  base::Value::Dict dict_to_insert;

  const base::Value::Dict* dict_value =
      local_settings.FindDict(supervised_user::kContentPackManualBehaviorHosts);
  if (dict_value) {
    dict_to_insert = dict_value->Clone();
  }
  dict_to_insert.Set(url.host(), isAllowed);
  settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts,
      std::move(dict_to_insert));
}
}  // namespace

@implementation SupervisedUserSettingsAppInterface : NSObject

+ (void)setSupervisedUserURLFilterBehavior:
    (supervised_user::SupervisedUserURLFilter::FilteringBehavior)behavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(behavior));
  if (behavior ==
      supervised_user::SupervisedUserURLFilter::FilteringBehavior::ALLOW) {
    settings_service->SetLocalSetting(supervised_user::kSafeSitesEnabled,
                                      base::Value(true));
  }
}

+ (void)resetSupervisedUserURLFilterBehavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior);
}

+ (void)resetManualUrlFiltering {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts);
}

+ (void)setFakePermissionCreator {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  CHECK(settings_service);
  std::unique_ptr<supervised_user::PermissionRequestCreator> creator =
      std::make_unique<supervised_user::PermissionRequestCreatorMock>(
          *settings_service);
  supervised_user::PermissionRequestCreatorMock* mocked_creator =
      static_cast<supervised_user::PermissionRequestCreatorMock*>(
          creator.get());
  mocked_creator->SetEnabled();

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  CHECK(service);
  service->remote_web_approvals_manager().ClearApprovalRequestsCreators();
  service->remote_web_approvals_manager().AddApprovalRequestCreator(
      std::move(creator));
}

+ (void)approveWebsiteDomain:(NSURL*)url {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RecordLocalWebsiteApproval(net::GURLWithNSURL(url).host());
}

+ (void)setFilteringToAllowAllSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::
                                               SupervisedUserURLFilter::ALLOW];
}

+ (void)setFilteringToAllowApprovedSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::
                                               SupervisedUserURLFilter::BLOCK];
}

+ (void)addWebsiteToAllowList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), true);
}

+ (void)addWebsiteToBlockList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), false);
}

+ (void)resetFirstTimeBanner {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromBrowserState(
      chrome_test_util::GetOriginalBrowserState());
  PrefService* user_prefs = browser_state->GetPrefs();
  CHECK(user_prefs);
  user_prefs->SetInteger(
      prefs::kFirstTimeInterstitialBannerState,
      static_cast<int>(
          supervised_user::FirstTimeInterstitialBannerState::kNeedToShow));
}

@end
