// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(SiteIsolationPolicyTest, DisableSiteIsolationSwitch) {
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

  // Error page isolation should not be affected by --disable-site-isolation-...
  // switches.
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
}

#if BUILDFLAG(IS_ANDROID)
// Since https://crbug.com/910273, the kDisableSiteIsolationForPolicy switch is
// only available/used on Android.
TEST(SiteIsolationPolicyTest, DisableSiteIsolationForPolicySwitch) {
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).  The test is still valid if
  // SitePerProcess is the default (e.g. via ContentBrowserClient's
  // ShouldEnableStrictSiteIsolation method) - don't skip the test in such case.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(SiteIsolationPolicy::AreIsolatedOriginsEnabled());
  EXPECT_FALSE(SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled());

  // Error page isolation should not be affected by --disable-site-isolation-...
  // switches.
  EXPECT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(true));
}
#endif

class ApplicationIsolationEnablingBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(
      BrowserContext* browser_context,
      const GURL& url,
      bool origin_matches_flag) override {
    return origin_matches_flag || url.SchemeIs("isolated-app");
  }
};

class SiteIsolationPolicyIsolatedApplicationTest : public testing::Test {
 public:
  void SetUp() override {
    SiteIsolationPolicy::DisableFlagCachingForTesting();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override { SetBrowserClientForTesting(old_client_); }

 private:
  BrowserTaskEnvironment task_environment_;
  ApplicationIsolationEnablingBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_;
};

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, Disabled) {
  GURL origin_url("https://www.bar.com");

  EXPECT_FALSE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, origin_url));
}

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, MatchingOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, "https://www.foo.com,https://www.bar.com");

  GURL origin_url("https://www.bar.com");
  EXPECT_TRUE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, origin_url));
}

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, NotMatchingOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, "https://www.foo.com,https://www.bar.com");

  GURL origin_url("https://www.not-allowed.com");
  EXPECT_FALSE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, origin_url));
}

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, InvalidOrigin) {
  std::string origin_string = "hdsdhdfhdh";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, origin_string);

  // Fails to convert into an origin, which leads to an empty origin.
  GURL origin_url(origin_string);
  EXPECT_FALSE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, origin_url));
}

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, FlagTypo) {
  // Verifies user typo in the origin for the command line flag
  // doesn't accidentally allow all origins.

  std::string invalid_origin_string = "htps://www.app.com";
  std::string valid_origin_string = "https://www.app.com";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, invalid_origin_string);

  GURL valid_origin_url(valid_origin_string);
  EXPECT_FALSE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, valid_origin_url));
}

TEST_F(SiteIsolationPolicyIsolatedApplicationTest, PortRemoved) {
  // Verifies that ports given to kIsolatedAppOrigins are ignored, and all
  // ports on the provided scheme+hostname pair will gain restricted API access.
  std::string origin_string = "https://app.com:1234";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kIsolatedAppOrigins, origin_string);

  EXPECT_TRUE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, GURL(origin_string)));
  EXPECT_TRUE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, GURL("https://app.com")));
  EXPECT_TRUE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, GURL("https://app.com:443")));
}

TEST_F(
    SiteIsolationPolicyIsolatedApplicationTest,
    ShouldSchemeUseApplicationIsolationLevelOverridesIsolatedAppOriginsFlag) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kIsolatedAppOrigins);

  // For the format of isolated app identifier see
  // https://github.com/WICG/isolated-web-apps/blob/main/Scheme.md
  EXPECT_TRUE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr,
      GURL(
          R"(isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic)")));
}

TEST_F(
    SiteIsolationPolicyIsolatedApplicationTest,
    ShouldSchemeUseApplicationIsolationLevelIsDisableForNonIsolatedAppScheme) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kIsolatedAppOrigins);

  EXPECT_FALSE(SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      /*browser_context=*/nullptr, GURL("http://example.com")));
}

}  // namespace content
