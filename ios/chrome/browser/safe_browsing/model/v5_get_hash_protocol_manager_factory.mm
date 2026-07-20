// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_get_hash_protocol_manager_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "build/branding_buildflags.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#import "ios/chrome/browser/safe_browsing/model/v5_search_hashes_cache_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr std::string_view kClientName = "googlechrome";
#else
inline constexpr std::string_view kClientName = "chromium";
#endif
}  // namespace

// static
safe_browsing::V5GetHashProtocolManager*
V5GetHashProtocolManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::V5GetHashProtocolManager>(
          profile, /*create=*/true);
}

// static
V5GetHashProtocolManagerFactory*
V5GetHashProtocolManagerFactory::GetInstance() {
  static base::NoDestructor<V5GetHashProtocolManagerFactory> instance;
  return instance.get();
}

V5GetHashProtocolManagerFactory::V5GetHashProtocolManagerFactory()
    : ProfileKeyedServiceFactoryIOS("V5GetHashProtocolManager",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(V5SearchHashesCacheFactory::GetInstance());
}

std::unique_ptr<KeyedService>
V5GetHashProtocolManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  // TODO(crbug.com/362791941): handle v4 references
  return std::make_unique<safe_browsing::V5GetHashProtocolManager>(
      safe_browsing_service->GetURLLoaderFactory(),
      safe_browsing::GetV4ProtocolConfig(std::string(kClientName),
                                         /*disable_auto_update=*/false),
      V5SearchHashesCacheFactory::GetForProfile(profile));
}
