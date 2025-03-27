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
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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
  return GetInstance()->GetServiceForProfileAs<ShoppingService>(
      profile, /*create=*/true);
}

// static
ShoppingService* ShoppingServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ShoppingService>(
      profile, /*create=*/false);
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ShoppingService",
                                    TestingCreation::kNoServiceForTests) {
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
  ProfileIOS* profile = ProfileIOS::FromBrowserState(state);
  PrefService* pref_service = profile->GetPrefs();

  return std::make_unique<ShoppingService>(
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      GetApplicationContext()->GetApplicationLocale(),
      ios::BookmarkModelFactory::GetForProfile(profile),
      OptimizationGuideServiceFactory::GetForProfile(profile), pref_service,
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile),
      PowerBookmarkServiceFactory::GetForProfile(profile), nullptr,
      /**ProductSpecificationsService not currently used on iOS
         crbug.com/329431295 */
      nullptr,
      nullptr, /** Cart and discount features are not available on iOS. */
      SessionProtoDBFactory<
          parcel_tracking_db::ParcelTrackingContent>::GetInstance()
          ->GetForProfile(profile),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<commerce::WebExtractorImpl>(),
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile));
}

}  // namespace commerce
