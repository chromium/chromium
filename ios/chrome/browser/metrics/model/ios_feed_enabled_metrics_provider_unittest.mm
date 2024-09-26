// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_enabled_metrics_provider.h"

#import <sstream>

#import "base/functional/overloaded.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/feed/core/shared_prefs/pref_names.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {

// Represents the name of a disabled preference.
struct DisabledPref {
  const char* name;
};

// Represents the FeedAblation feature enabled.
struct FeedAblationEnabled {};

// Represents the FeedAblation feature disabled with maybe one pref disabled.
using FeedAblationDisabled = std::optional<DisabledPref>;

// Represents the configuration.
using FeedMetricsConfig =
    std::variant<FeedAblationEnabled, FeedAblationDisabled>;

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const DisabledPref& config) {
  if (config.name == prefs::kArticlesForYouEnabled) {
    return "PrefArticlesForYouDisabled";
  }
  if (config.name == feed::prefs::kArticlesListVisible) {
    return "PrefArticlesListVisibleDisabled";
  }
  if (config.name == prefs::kNTPContentSuggestionsEnabled) {
    return "PrefNTPContentSuggestionsDisabled";
  }

  NOTREACHED();
}

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const FeedAblationDisabled& config) {
  std::string result = "FeedAblationDisabled";
  if (!config.has_value()) {
    return result;
  }

  return result + "_" + PrintToString(config.value());
}

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const FeedMetricsConfig& param) {
  return std::visit(
      base::Overloaded{[](const FeedAblationEnabled&) -> std::string {
                         return "FeedAblationEnabled";
                       },
                       [](const FeedAblationDisabled& config) {
                         return PrintToString(config);
                       }},
      param);
}

}  // namespace

// Tests metrics that are recorded by IOSFeedEnabledMetricsProvider.
class IOSFeedEnabledMetricsProviderTest
    : public testing::TestWithParam<FeedMetricsConfig> {
 public:
  IOSFeedEnabledMetricsProviderTest() {
    if (IsFeedAblationEnabled()) {
      feature_list_.InitAndEnableFeature(kEnableFeedAblation);
    } else {
      feature_list_.InitAndDisableFeature(kEnableFeedAblation);
    }
  }

  // Returns whether the feature kEnableFeedAblation is enabled for the test.
  bool IsFeedAblationEnabled() const {
    return std::visit(
        base::Overloaded{[](const FeedAblationEnabled&) { return true; },
                         [](const FeedAblationDisabled&) { return false; }},
        GetParam());
  }

  // Returns whether the test is configured to enabled or disable displaying
  // of the feed (by default).
  bool CanDisplayFeed() const {
    return std::visit(
        base::Overloaded{[](const FeedAblationEnabled&) { return false; },
                         [](const FeedAblationDisabled& config) {
                           return !config.has_value();
                         }},
        GetParam());
  }

  // Creates a new TestProfileIOS with `name` and configured with
  // `config` (possibly disabling some preferences).
  void CreateBrowserState(const std::string& name,
                          const FeedMetricsConfig& param) {
    ProfileIOS* profile = profile_manager_.AddProfileWithBuilder(
        std::move(TestProfileIOS::Builder().SetName(name)));

    PrefService* prefs = profile->GetPrefs();
    return std::visit(
        base::Overloaded{[](const FeedAblationEnabled&) {},
                         [prefs](const FeedAblationDisabled& config) {
                           if (config.has_value()) {
                             prefs->SetBoolean(config->name, false);
                           }
                         }},
        param);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    IOSFeedEnabledMetricsProviderTestInstantiation,
    IOSFeedEnabledMetricsProviderTest,
    ::testing::Values(
        FeedAblationEnabled{},
        FeedAblationDisabled{},
        FeedAblationDisabled{DisabledPref{prefs::kArticlesForYouEnabled}},
        FeedAblationDisabled{DisabledPref{feed::prefs::kArticlesListVisible}},
        FeedAblationDisabled{
            DisabledPref{prefs::kNTPContentSuggestionsEnabled}}),
    ::testing::PrintToStringParamName());

// Tests the implementation of ProvideCurrentSessionData.
TEST_P(IOSFeedEnabledMetricsProviderTest,
       ProvideCurrentSessionData_NoBrowserState) {
  IOSFeedEnabledMetricsProvider provider;
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);

  // Check that no value is logged.
  EXPECT_THAT(histogram_tester().GetAllSamples(kFeedEnabledHistogram),
              ::testing::ElementsAre());
}

// Tests the implementation of ProvideCurrentSessionData.
TEST_P(IOSFeedEnabledMetricsProviderTest,
       ProvideCurrentSessionData_OneBrowserState) {
  CreateBrowserState("Default", GetParam());

  IOSFeedEnabledMetricsProvider provider;
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);

  // Check that the expected value is logged.
  EXPECT_THAT(histogram_tester().GetAllSamples(kFeedEnabledHistogram),
              ::testing::ElementsAre(base::Bucket(CanDisplayFeed(), 1)));
}

// Tests the implementation of ProvideCurrentSessionData.
TEST_P(IOSFeedEnabledMetricsProviderTest,
       ProvideCurrentSessionData_MultipleBrowserStates) {
  CreateBrowserState("Profile1", GetParam());
  CreateBrowserState("Profile2", GetParam());
  CreateBrowserState("Profile3", FeedMetricsConfig{FeedAblationDisabled{}});

  IOSFeedEnabledMetricsProvider provider;
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);

  // Check that the expected values are logged.
  if (!IsFeedAblationEnabled() && !CanDisplayFeed()) {
    // This corresponds to the case where "Profile3" is the only BrowserState
    // where the feed can be displayed and the feed ablation feature is not
    // enabled.
    EXPECT_THAT(
        histogram_tester().GetAllSamples(kFeedEnabledHistogram),
        ::testing::ElementsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  } else {
    EXPECT_THAT(histogram_tester().GetAllSamples(kFeedEnabledHistogram),
                ::testing::ElementsAre(base::Bucket(CanDisplayFeed(), 3)));
  }
}
