// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"

#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#import "components/commerce/core/shopping_service.h"
#import "components/commerce/ios/browser/web_extractor_impl.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

// static
ShoppingServiceFactory* ShoppingServiceFactory::GetInstance() {
  static base::NoDestructor<ShoppingServiceFactory> instance;
  return instance.get();
}

// static
ShoppingService* ShoppingServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
ShoppingService* ShoppingServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ShoppingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
                GetInstance());
  DependsOn(SessionProtoDBFactory<
            parcel_tracking_db::ParcelTrackingContent>::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> ShoppingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(state);
  PrefService* pref_service = profile ? profile->GetPrefs() : nullptr;

  if (IsIOSParcelTrackingEnabled()) {
    RecordParcelTrackingOptInStatus(pref_service);
  }

  return std::make_unique<ShoppingService>(
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      GetApplicationContext()->GetApplicationLocale(),
      ios::BookmarkModelFactory::GetForProfile(profile),
      OptimizationGuideServiceFactory::GetForProfile(profile),
      pref_service, IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile),
      PowerBookmarkServiceFactory::GetForProfile(profile), nullptr,
      nullptr, /**ProductSpecificationsService not currently used on iOS
                  b/329431295 */
      SessionProtoDBFactory<
          parcel_tracking_db::ParcelTrackingContent>::GetInstance()
          ->GetForProfile(profile),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<commerce::WebExtractorImpl>(),
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile),
      ios::TemplateURLServiceFactory::GetForProfile(profile));
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
