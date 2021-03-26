// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
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

#if defined(OS_ANDROID)
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

}  // namespace content
