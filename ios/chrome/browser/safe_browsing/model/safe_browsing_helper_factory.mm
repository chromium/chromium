// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_helper_factory.h"

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_helper.h"

// static
SafeBrowsingHelperFactory* SafeBrowsingHelperFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingHelperFactory> instance;
  return instance.get();
}

// static
SafeBrowsingHelper* SafeBrowsingHelperFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SafeBrowsingHelper>(
      profile, /*create=*/true);
}

SafeBrowsingHelperFactory::SafeBrowsingHelperFactory()
    : ProfileKeyedServiceFactoryIOS("SafeBrowsingHelper",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(SafeBrowsingMetricsCollectorFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SafeBrowsingHelperFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<SafeBrowsingHelper>(
      profile->GetPrefs(), GetApplicationContext()->GetSafeBrowsingService(),
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile));
}
