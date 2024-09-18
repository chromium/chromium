// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"

// static
safe_browsing::SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
safe_browsing::SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<safe_browsing::SafeBrowsingMetricsCollector*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SafeBrowsingMetricsCollectorFactory*
SafeBrowsingMetricsCollectorFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingMetricsCollectorFactory> instance;
  return instance.get();
}

SafeBrowsingMetricsCollectorFactory::SafeBrowsingMetricsCollectorFactory()
    : BrowserStateKeyedServiceFactory(
          "SafeBrowsingMetricsCollector",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
SafeBrowsingMetricsCollectorFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::SafeBrowsingMetricsCollector>(
      profile->GetPrefs());
}
