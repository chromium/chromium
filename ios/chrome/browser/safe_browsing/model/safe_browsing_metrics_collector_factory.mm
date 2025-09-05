// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"

#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"

// static
safe_browsing::SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::SafeBrowsingMetricsCollector>(
          profile, /*create=*/true);
}

// static
SafeBrowsingMetricsCollectorFactory*
SafeBrowsingMetricsCollectorFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingMetricsCollectorFactory> instance;
  return instance.get();
}

SafeBrowsingMetricsCollectorFactory::SafeBrowsingMetricsCollectorFactory()
    : ProfileKeyedServiceFactoryIOS("SafeBrowsingMetricsCollector") {}

std::unique_ptr<KeyedService>
SafeBrowsingMetricsCollectorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<safe_browsing::SafeBrowsingMetricsCollector>(
      profile->GetPrefs());
}
