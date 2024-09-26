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

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserStateIfExists(
    ProfileIOS* profile) {
  return GetForProfileIfExists(profile);
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
}

std::unique_ptr<KeyedService> ShoppingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ProfileIOS* chrome_state = ProfileIOS::FromBrowserState(state);
  PrefService* pref_service = chrome_state ? chrome_state->GetPrefs() : nullptr;

  if (IsIOSParcelTrackingEnabled()) {
    RecordParcelTrackingOptInStatus(pref_service);
  }

  return std::make_unique<ShoppingService>(
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      GetApplicationContext()->GetApplicationLocale(),
      ios::BookmarkModelFactory::GetForProfile(chrome_state),
      OptimizationGuideServiceFactory::GetForProfile(chrome_state),
      pref_service, IdentityManagerFactory::GetForProfile(chrome_state),
      SyncServiceFactory::GetForProfile(chrome_state),
      chrome_state->GetSharedURLLoaderFactory(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(chrome_state),
      PowerBookmarkServiceFactory::GetForProfile(chrome_state), nullptr,
      nullptr, /**ProductSpecificationsService not currently used on iOS
                  b/329431295 */
      SessionProtoDBFactory<
          parcel_tracking_db::ParcelTrackingContent>::GetInstance()
          ->GetForProfile(chrome_state),
      ios::HistoryServiceFactory::GetForProfile(
          chrome_state, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<commerce::WebExtractorImpl>(),
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(chrome_state));
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
