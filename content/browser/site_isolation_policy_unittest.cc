// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include "base/command_line.h"
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

}  // namespace content
