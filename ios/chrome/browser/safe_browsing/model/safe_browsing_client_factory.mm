// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_helper_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/web/public/browser_state.h"

namespace {

// Returns the Url lookup service used by Safe Browsing for `profile`. The
// consumer or enterprise service is returned depending on the
// EnterpriseRealTimeUrlCheckMode policy.
safe_browsing::RealTimeUrlLookupServiceBase* GetUrlLookupService(
    ProfileIOS* profile) {
  // TODO(crbug.com/399376991): Return Enterprise service when
  // EnterpriseRealTimeUrlCheckMode policy is enabled.
  return RealTimeUrlLookupServiceFactory::GetForProfile(profile);
}

}  // namespace

// static
SafeBrowsingClient* SafeBrowsingClientFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SafeBrowsingClient>(
      profile, /*create=*/true);
}

// static
SafeBrowsingClientFactory* SafeBrowsingClientFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingClientFactory> instance;
  return instance.get();
}

SafeBrowsingClientFactory::SafeBrowsingClientFactory()
    : ProfileKeyedServiceFactoryIOS("SafeBrowsingClient",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(HashRealTimeServiceFactory::GetInstance());
  DependsOn(PrerenderServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SafeBrowsingClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  safe_browsing::HashRealTimeService* hash_real_time_service = nullptr;
  if (base::FeatureList::IsEnabled(safe_browsing::kHashPrefixRealTimeLookups)) {
    hash_real_time_service = HashRealTimeServiceFactory::GetForProfile(profile);
  }
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForProfile(profile);
  return std::make_unique<SafeBrowsingClientImpl>(
      profile->GetPrefs(), hash_real_time_service, prerender_service,
      // base::Unretained is safe because the RealTimeUrlLookupServiceBase will
      // be destroyed before the profile it is attached to.
      base::BindRepeating(&GetUrlLookupService, base::Unretained(profile)));
}
