// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service_factory.h"

#import "components/policy/core/browser/url_list/policy_blocklist_service.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
PolicyBlocklistServiceFactory* PolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<PolicyBlocklistServiceFactory> instance;
  return instance.get();
}

// static
PolicyBlocklistService* PolicyBlocklistServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PolicyBlocklistService>(
      profile, /*create=*/true);
}

PolicyBlocklistServiceFactory::PolicyBlocklistServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PolicyBlocklist",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

PolicyBlocklistServiceFactory::~PolicyBlocklistServiceFactory() = default;

std::unique_ptr<KeyedService>
PolicyBlocklistServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  PrefService* prefs = profile->GetPrefs();
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      prefs, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  std::unique_ptr<policy::URLBlocklistManager> incognito_url_blocklist_manager;
  if (profile->IsOffTheRecord()) {
    incognito_url_blocklist_manager =
        std::make_unique<policy::URLBlocklistManager>(
            prefs, policy::policy_prefs::kIncognitoModeBlocklist,
            policy::policy_prefs::kIncognitoModeAllowlist);
  }
  return std::make_unique<PolicyBlocklistService>(
      std::move(url_blocklist_manager),
      std::move(incognito_url_blocklist_manager), prefs);
}
