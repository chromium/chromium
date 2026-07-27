// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/subscription_eligibility/model/ios_subscription_eligibility_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/pref_service.h"
#import "components/subscription_eligibility/subscription_eligibility_prefs.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/synthetic_trial_registry.h"
#import "components/variations/synthetic_trials.h"
#import "components/variations/synthetic_trials_active_group_id_provider.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/subscription_eligibility/model/subscription_eligibility_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace subscription_eligibility {

class IOSSubscriptionEligibilityMetricsProviderTest : public PlatformTest {
 public:
  IOSSubscriptionEligibilityMetricsProviderTest() = default;
  ~IOSSubscriptionEligibilityMetricsProviderTest() override = default;
  void SetUp() override { PlatformTest::SetUp(); }

  void TearDown() override { PlatformTest::TearDown(); }

  void SetAiSubscriptionTierForProfile(int32_t subscription_tier,
                                       ProfileIOS* profile) {
    profile->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier,
        subscription_tier);
  }

 protected:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
};

TEST_F(IOSSubscriptionEligibilityMetricsProviderTest,
       ProvideCurrentSessionData) {
  TestProfileIOS::Builder builder1;
  builder1.SetName("Profile1");
  TestProfileIOS* profile1 =
      profile_manager_.AddProfileWithBuilder(std::move(builder1));

  IOSSubscriptionEligibilityMetricsProvider provider;

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1, profile1);
    metrics::ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        AiSubscriptionTierStatus::kAllProfilesAtTierEquals1, 1);
  }

  // Add another profile.
  TestProfileIOS::Builder builder2;
  builder2.SetName("Profile2");
  TestProfileIOS* profile2 =
      profile_manager_.AddProfileWithBuilder(std::move(builder2));

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(1, profile1);
    SetAiSubscriptionTierForProfile(2, profile2);
    metrics::ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers, 1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAiSubscriptionTierForProfile(0, profile1);
    SetAiSubscriptionTierForProfile(1, profile2);
    metrics::ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);
    histogram_tester.ExpectUniqueSample(
        "SubscriptionEligibility.AiSubscriptionTierStatus",
        AiSubscriptionTierStatus::kSomeProfilesSubscribed, 1);
  }
}

}  // namespace subscription_eligibility
