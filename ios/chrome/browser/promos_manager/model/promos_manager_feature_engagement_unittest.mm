// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/configuration.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/group_constants.h"
#import "components/feature_engagement/public/ios_promo_feature_configuration.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "testing/platform_test.h"

// Unittests related to the triggering of PromosManager features.
class PromosManagerFeatureEngagementTest : public PlatformTest {
 public:
  PromosManagerFeatureEngagementTest() {}
  ~PromosManagerFeatureEngagementTest() override {}

  std::vector<const base::Feature*> ActiveStandardPromos() {
    return {
        &feature_engagement::kIPHiOSPromoAppStoreFeature,
        &feature_engagement::kIPHiOSPromoWhatsNewFeature,
        &feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature,
        &feature_engagement::kIPHiOSDockingPromoFeature,
    };
  }
};

// Tests that the per-promo limit preventing a given promo from triggering more
// than once in 30 days is added to all active standard promos.
TEST_F(PromosManagerFeatureEngagementTest, TestPerPromoLimit) {
  for (const base::Feature* feature : ActiveStandardPromos()) {
    std::optional<feature_engagement::FeatureConfig> optional_config =
        feature_engagement::GetClientSideiOSPromoFeatureConfig(feature);

    ASSERT_TRUE(optional_config.has_value());
    feature_engagement::FeatureConfig config = optional_config.value();
    bool contains_one_in_thirty_limit = false;
    for (auto event_config : config.event_configs) {
      if (event_config.name == config.trigger.name &&
          event_config.comparator ==
              feature_engagement::Comparator(feature_engagement::EQUAL, 0) &&
          event_config.window == 30) {
        contains_one_in_thirty_limit = true;
      }
    }
    EXPECT_TRUE(contains_one_in_thirty_limit);
  }
}

// Tests that the group rules for global promo limits have been added to all
// active standard promos.
TEST_F(PromosManagerFeatureEngagementTest, TestGlobalLimit) {
  for (const base::Feature* feature : ActiveStandardPromos()) {
    std::optional<feature_engagement::FeatureConfig> optional_config =
        feature_engagement::GetClientSideiOSPromoFeatureConfig(feature);

    ASSERT_TRUE(optional_config.has_value());
    feature_engagement::FeatureConfig config = optional_config.value();
    bool contains_promo_group = false;
    for (auto group : config.groups) {
      if (group == feature_engagement::kiOSFullscreenPromosGroup.name) {
        contains_promo_group = true;
      }
    }
    EXPECT_TRUE(contains_promo_group);
  }
}
