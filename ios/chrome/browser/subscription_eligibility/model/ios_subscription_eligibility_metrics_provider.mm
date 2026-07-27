// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/subscription_eligibility/model/ios_subscription_eligibility_metrics_provider.h"

#import <set>
#import <vector>

#import "base/metrics/histogram_functions.h"
#import "components/subscription_eligibility/subscription_eligibility_service.h"
#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/subscription_eligibility/model/subscription_eligibility_service_factory.h"

namespace subscription_eligibility {

IOSSubscriptionEligibilityMetricsProvider::
    IOSSubscriptionEligibilityMetricsProvider() = default;

IOSSubscriptionEligibilityMetricsProvider::
    ~IOSSubscriptionEligibilityMetricsProvider() = default;

void IOSSubscriptionEligibilityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  if (!profile_manager) {
    return;
  }

  std::vector<ProfileIOS*> profile_list = profile_manager->GetLoadedProfiles();
  if (profile_list.empty()) {
    return;
  }

  std::set<int32_t> subscription_tiers;
  for (auto* profile : profile_list) {
    auto* subscription_eligibility_service =
        SubscriptionEligibilityServiceFactory::GetForProfile(profile);
    int32_t profile_subscription_tier =
        subscription_eligibility_service
            ? subscription_eligibility_service->GetAiSubscriptionTier()
            : 0;
    subscription_tiers.insert(
        profile_subscription_tier >= 0 ? profile_subscription_tier : 0);
  }

  AiSubscriptionTierStatus status =
      ComputeAiSubscriptionTierStatus(subscription_tiers);

  CHECK_NE(status, AiSubscriptionTierStatus::kValueNotSet);
  base::UmaHistogramEnumeration(kAiSubscriptionTierStatusHistogramName, status);

  std::string group_name = GetSyntheticTrialGroupName(status);
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kAiSubscriptionTierSyntheticTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace subscription_eligibility
