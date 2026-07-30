// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_preview_data_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/signin/core/browser/account_preview_data_service_impl.h"
#import "components/signin/ios/browser/wait_for_network_callback_helper_ios.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/metrics/model/ios_profile_metrics_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
signin::AccountPreviewDataService*
AccountPreviewDataServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<signin::AccountPreviewDataService>(
          profile, /*create=*/true);
}

// static
AccountPreviewDataServiceFactory*
AccountPreviewDataServiceFactory::GetInstance() {
  static base::NoDestructor<AccountPreviewDataServiceFactory> instance;
  return instance.get();
}

AccountPreviewDataServiceFactory::AccountPreviewDataServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AccountPreviewDataService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSProfileMetricsServiceFactory::GetInstance());
}

AccountPreviewDataServiceFactory::~AccountPreviewDataServiceFactory() = default;

std::unique_ptr<KeyedService>
AccountPreviewDataServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  PrefService* prefs = profile->GetPrefs();
  if (!base::FeatureList::IsEnabled(switches::kEnableAccountPreviewData) ||
      // Since this is a managed preference, it is fine for it be checked only
      // once per session.
      // TODO(crbug.com/540713764): Consider moving this condition to the
      // service itself, as well as checking the local preference value instead.
      !prefs->GetBoolean(prefs::kSigninAllowed)) {
    return nullptr;
  }

  metrics::ProfileMetricsService* profile_metrics_service =
      IOSProfileMetricsServiceFactory::GetForProfile(profile);

  return std::make_unique<signin::AccountPreviewDataServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile), prefs,
      profile->GetSharedURLLoaderFactory(),
      std::make_unique<WaitForNetworkCallbackHelperIOS>(), ::GetChannel(),
      profile_metrics_service);
}

void AccountPreviewDataServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::AccountPreviewDataService::RegisterProfilePrefs(registry);
}
