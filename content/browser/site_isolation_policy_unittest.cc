// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// A ContentBrowserClient that isolates error pages in all frames, regardless
// of whether features::kIsolateSubframeErrorPages is enabled, and that allows
// tests to disable partial site isolation, similarly to how chrome/ does it for
// low-memory Android devices.
class ErrorPageIsolationBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldIsolateErrorPage(bool in_main_frame) override { return true; }

  bool ShouldDisableSiteIsolation(
      SiteIsolationMode site_isolation_mode) override {
    return site_isolation_mode == SiteIsolationMode::kPartialSiteIsolation &&
           disable_partial_site_isolation_;
  }

  void set_disable_partial_site_isolation(bool disable) {
    disable_partial_site_isolation_ = disable;
  }

 private:
  bool disable_partial_site_isolation_ = false;
};

}  // namespace

class SiteIsolationPolicyTest : public testing::Test {
 protected:
  ErrorPageIsolationBrowserClient& browser_client() { return browser_client_; }

 private:
  ErrorPageIsolationBrowserClient browser_client_;
  ScopedContentBrowserClientSetting browser_client_setting_{&browser_client_};
};

TEST_F(SiteIsolationPolicyTest, DisableSiteIsolationSwitch) {
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).  The test is still valid if
  // SitePerProcess is the default (e.g. via ContentBrowserClient's
  // ShouldEnableStrictSiteIsolation method) - don't skip the test in such case.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  SiteIsolationPolicy::DisableFlagCachingForTesting();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kDisableSiteIsolation);
  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(SiteIsolationPolicy::AreIsolatedOriginsEnabled());
  EXPECT_FALSE(SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled());

  // Main frame error page isolation should not be affected by
  // --disable-site-isolation-... switches, but subframe error page isolation
  // should be disabled.
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
  EXPECT_FALSE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(false));
}

#if BUILDFLAG(IS_ANDROID)
// Since https://crbug.com/910273, the kDisableSiteIsolationForPolicy switch is
// only available/used on Android.
TEST_F(SiteIsolationPolicyTest, DisableSiteIsolationForPolicySwitch) {
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).  The test is still valid if
  // SitePerProcess is the default (e.g. via ContentBrowserClient's
  // ShouldEnableStrictSiteIsolation method) - don't skip the test in such case.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  SiteIsolationPolicy::DisableFlagCachingForTesting();
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(SiteIsolationPolicy::AreIsolatedOriginsEnabled());
  EXPECT_FALSE(SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled());

  // Main frame error page isolation should not be affected by
  // --disable-site-isolation-... switches, but subframe error page isolation
  // should be disabled.
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
  EXPECT_FALSE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(false));
}
#endif

// Check that subframe error page isolation is turned off when the embedder
// disables partial site isolation (e.g., when chrome/ is running on a
// low-memory Android device), unless it's explicitly enabled from the command
// line.
TEST_F(SiteIsolationPolicyTest, SubframeErrorPageIsolationDisabledByEmbedder) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteIsolation)) {
    GTEST_SKIP();
  }

  // Start with a clean FeatureList, so that the results aren't affected by any
  // command-line overrides of features::kIsolateSubframeErrorPages.
  base::test::ScopedFeatureList empty_feature_list;
  empty_feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(false));

  // Disable partial site isolation. This is how the content/ embedder applies
  // the memory threshold on Android. This should only affect subframes.
  browser_client().set_disable_partial_site_isolation(true);
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
  EXPECT_FALSE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(false));

  // Simulate enabling subframe error page isolation from the command line.
  // (Note that InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that triggering the feature via chrome://flags follows the
  // same override path as well.) This should take precedence over the embedder
  // disabling partial site isolation.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIsolateSubframeErrorPages);
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(false));
}

class ApplicationIsolationEnablingBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url.SchemeIs("isolated-app");
  }
};

namespace {

// A ContentBrowserClient that controls whether full site isolation is enabled
// (as chrome/ does for the kSitePerProcess feature and Advanced Protection),
// and whether the memory threshold for full site isolation disables it.
class AdsExclusionBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldEnableStrictSiteIsolation() override {
    return enable_strict_site_isolation_;
  }

  bool ShouldDisableSiteIsolation(
      SiteIsolationMode site_isolation_mode) override {
    return site_isolation_mode == SiteIsolationMode::kStrictSiteIsolation &&
           below_strict_memory_threshold_;
  }

  void set_enable_strict_site_isolation(bool enable) {
    enable_strict_site_isolation_ = enable;
  }

  void set_below_strict_memory_threshold(bool below) {
    below_strict_memory_threshold_ = below;
  }

 private:
  bool enable_strict_site_isolation_ = false;
  bool below_strict_memory_threshold_ = false;
};

// Lets tests simulate a user or enterprise override of origin-keyed processes.
class OriginKeyedProcessesOverrideBrowserClient : public ContentBrowserClient {
 public:
  std::optional<bool> GetOverrideValueForOriginKeyedProcesses() override {
    return override_value_;
  }

  void set_override_value(std::optional<bool> value) {
    override_value_ = value;
  }

 private:
  std::optional<bool> override_value_;
};

}  // namespace

// Tests for SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds().
// The fixture starts from partial site isolation with the feature enabled.
class SiteIsolationPolicyAdsExclusionTest : public testing::Test {
 public:
  SiteIsolationPolicyAdsExclusionTest() {
    SiteIsolationPolicy::DisableFlagCachingForTesting();
    // Bots may run with --site-per-process. Start from partial site
    // isolation.
    scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
        switches::kSitePerProcess);
    feature_list_.InitWithFeatures({features::kExcludeAdsFromSiteIsolation,
                                    features::kDefaultSiteInstanceGroups},
                                   {features::kStrictOriginIsolation});
  }

 protected:
  AdsExclusionBrowserClient& browser_client() { return browser_client_; }
  base::CommandLine* command_line() {
    return scoped_command_line_.GetProcessCommandLine();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list_;
  AdsExclusionBrowserClient browser_client_;
  ScopedContentBrowserClientSetting browser_client_setting_{&browser_client_};
};

// With partial site isolation, the mode is enabled, and it is not reported as
// full site isolation.
TEST_F(SiteIsolationPolicyAdsExclusionTest, EnabledWithPartialSiteIsolation) {
  EXPECT_TRUE(SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(SiteIsolationPolicy::IsSitePerProcessOrStricter());
  EXPECT_EQ(SiteIsolationDisabledReason::kAdsExcludedFromSiteIsolation,
            SiteIsolationPolicy::GetSiteIsolationDisabledReason());
}

// The --site-per-process switch (also set by chrome://flags and the
// SitePerProcess enterprise policy) keeps full site isolation.
TEST_F(SiteIsolationPolicyAdsExclusionTest, SitePerProcessSwitchWins) {
  command_line()->AppendSwitch(switches::kSitePerProcess);
  EXPECT_TRUE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_TRUE(SiteIsolationPolicy::IsSitePerProcessOrStricter());
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

// Full site isolation enabled by the embedder (e.g. the kSitePerProcess
// feature or Advanced Protection in chrome/) is kept.
TEST_F(SiteIsolationPolicyAdsExclusionTest, EmbedderStrictSiteIsolationWins) {
  browser_client().set_enable_strict_site_isolation(true);
  EXPECT_TRUE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_TRUE(SiteIsolationPolicy::IsSitePerProcessOrStricter());
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

// Strict origin isolation is stricter than this mode, so it is kept.
TEST_F(SiteIsolationPolicyAdsExclusionTest, StrictOriginIsolationWins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStrictOriginIsolation);
  EXPECT_TRUE(SiteIsolationPolicy::IsSitePerProcessOrStricter());
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

// Devices below the memory threshold for full site isolation keep partial
// site isolation.
TEST_F(SiteIsolationPolicyAdsExclusionTest, StrictMemoryThreshold) {
  browser_client().set_below_strict_memory_threshold(true);
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

// The --disable-site-isolation-trials switch (also set by chrome://flags)
// turns the mode off.
TEST_F(SiteIsolationPolicyAdsExclusionTest, DisableSiteIsolationSwitch) {
  command_line()->AppendSwitch(switches::kDisableSiteIsolation);
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

#if BUILDFLAG(IS_ANDROID)
// Setting the SitePerProcessAndroid enterprise policy to false adds the
// --disable-site-isolation-for-policy switch, which turns off all site
// isolation, including this mode.
TEST_F(SiteIsolationPolicyAdsExclusionTest, DisableSiteIsolationForPolicy) {
  command_line()->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
  EXPECT_EQ(SiteIsolationDisabledReason::kDisabledByPolicy,
            SiteIsolationPolicy::GetSiteIsolationDisabledReason());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(SiteIsolationPolicyAdsExclusionTest, RequiresFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kExcludeAdsFromSiteIsolation);
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

TEST_F(SiteIsolationPolicyAdsExclusionTest, RequiresDefaultSiteInstanceGroups) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kDefaultSiteInstanceGroups);
  EXPECT_FALSE(
      SiteIsolationPolicy::UseDedicatedProcessesForAllSitesExceptAds());
}

// Tests for SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses().
class SiteIsolationPolicyOriginAdsExclusionTest : public testing::Test {
 public:
  SiteIsolationPolicyOriginAdsExclusionTest() {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kSitePerProcess);
    // Needed to create field trials.
    scoped_empty_feature_and_field_trial_lists_
        .InitWithEmptyFeatureAndFieldTrialLists();
  }

 protected:
  // How features::kExcludeAdsFromOriginIsolation is set.
  enum class ExclusionSource {
    kNone,
    kFieldTrial,
    kCommandLineEnabled,
    kCommandLineDisabled,
    // Feature<Trial on the command line. Counts as a field trial.
    kCommandLineWithTrial,
  };

  void InitFeatures(ExclusionSource exclusion_source) {
    const std::string exclusion_feature =
        features::kExcludeAdsFromOriginIsolation.name;
    std::string enabled_features =
        std::string(features::kOriginIsolationHeader.name) + "," +
        features::kOriginKeyedProcessesByDefault.name;
    std::string disabled_features;
    switch (exclusion_source) {
      case ExclusionSource::kNone:
      case ExclusionSource::kFieldTrial:
        break;
      case ExclusionSource::kCommandLineEnabled:
        enabled_features += "," + exclusion_feature;
        break;
      case ExclusionSource::kCommandLineDisabled:
        disabled_features = exclusion_feature;
        break;
      case ExclusionSource::kCommandLineWithTrial:
        base::FieldTrialList::CreateFieldTrial("ExcludeAdsStudy", "Enabled");
        enabled_features += "," + exclusion_feature + "<ExcludeAdsStudy";
        break;
    }

    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->InitFromCommandLine(enabled_features, disabled_features);
    if (exclusion_source == ExclusionSource::kFieldTrial) {
      feature_list->RegisterFieldTrialOverride(
          exclusion_feature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
          base::FieldTrialList::CreateFieldTrial("ExcludeAdsStudy", "Enabled"));
    }
    feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  OriginKeyedProcessesOverrideBrowserClient& browser_client() {
    return browser_client_;
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_empty_feature_and_field_trial_lists_;
  base::test::ScopedFeatureList feature_list_;
  OriginKeyedProcessesOverrideBrowserClient browser_client_;
  ScopedContentBrowserClientSetting browser_client_setting_{&browser_client_};
};

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest, FieldTrialWithoutOverride) {
  InitFeatures(ExclusionSource::kFieldTrial);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_TRUE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("ExcludeAdsStudy"));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest, OverrideWinsOverFieldTrial) {
  InitFeatures(ExclusionSource::kFieldTrial);
  browser_client().set_override_value(true);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
  // The override keeps the client out of the study.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("ExcludeAdsStudy"));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest,
       RequiresOriginAgentClustersByDefault) {
  InitFeatures(ExclusionSource::kFieldTrial);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOriginIsolationHeader);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  ASSERT_FALSE(SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest,
       CommandLineExclusionWithOverride) {
  InitFeatures(ExclusionSource::kCommandLineEnabled);
  browser_client().set_override_value(true);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_TRUE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest,
       CommandLineDisableWithOverride) {
  InitFeatures(ExclusionSource::kCommandLineDisabled);
  browser_client().set_override_value(true);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  ASSERT_TRUE(
      base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kExcludeAdsFromOriginIsolation.name));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest, CommandLineWithTrial) {
  InitFeatures(ExclusionSource::kCommandLineWithTrial);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  ASSERT_FALSE(
      base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kExcludeAdsFromOriginIsolation.name));
  EXPECT_TRUE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));

  browser_client().set_override_value(true);
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest,
       OverrideDisabledWithCommandLineExclusion) {
  InitFeatures(ExclusionSource::kCommandLineEnabled);
  browser_client().set_override_value(false);
  EXPECT_FALSE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest,
       OverrideDisabledWithFieldTrial) {
  InitFeatures(ExclusionSource::kFieldTrial);
  browser_client().set_override_value(false);
  EXPECT_FALSE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

TEST_F(SiteIsolationPolicyOriginAdsExclusionTest, RequiresFeature) {
  InitFeatures(ExclusionSource::kNone);
  ASSERT_TRUE(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
      /*browser_context=*/nullptr));
  EXPECT_FALSE(SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
      /*browser_context=*/nullptr));
}

}  // namespace content
