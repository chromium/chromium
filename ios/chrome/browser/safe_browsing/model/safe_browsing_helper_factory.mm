// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_helper_factory.h"

#import <utility>

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_helper.h"

SafeBrowsingHelperFactory::SafeBrowsingHelperFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "SafeBrowsingHelper",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(SafeBrowsingMetricsCollectorFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SafeBrowsingHelperFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<SafeBrowsingHelper>(
      profile->GetPrefs(), GetApplicationContext()->GetSafeBrowsingService(),
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile));
}
