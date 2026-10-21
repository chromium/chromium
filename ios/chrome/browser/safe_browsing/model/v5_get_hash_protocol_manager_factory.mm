// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_get_hash_protocol_manager_factory.h"

#import <memory>
#import <utility>

#import "build/branding_buildflags.h"
#import "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#import "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
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

V5GetHashProtocolManagerFactory::V5GetHashProtocolManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "V5GetHashProtocolManager",
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
  return std::make_unique<safe_browsing::V5GetHashProtocolManager>(
      safe_browsing_service->GetURLLoaderFactory(),
      safe_browsing::GetSBProtocolConfig(std::string(kClientName),
                                         /*disable_auto_update=*/false),
      V5SearchHashesCacheFactory::GetForProfile(profile),
      /*webui_delegate=*/safe_browsing::WebUIIOSInfoSingleton::GetInstance());
}
