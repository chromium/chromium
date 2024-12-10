// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service_factory.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
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
    web::BrowserState* browser_state) const {
  PrefService* prefs = ProfileIOS::FromBrowserState(browser_state)->GetPrefs();
  return std::make_unique<PolicyBlocklistService>(
      std::make_unique<policy::URLBlocklistManager>(
          prefs, policy::policy_prefs::kUrlBlocklist,
          policy::policy_prefs::kUrlAllowlist));
}
