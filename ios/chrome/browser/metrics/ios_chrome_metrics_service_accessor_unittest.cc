// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#include "build/branding_buildflags.h"

#include "base/macros.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/chrome/test/testing_application_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class IOSChromeMetricsServiceAccessorTest : public PlatformTest {
 public:
  IOSChromeMetricsServiceAccessorTest() {}

  PrefService* GetLocalState() { return local_state_.Get(); }

 private:
  IOSChromeScopedTestingLocalState local_state_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeMetricsServiceAccessorTest);
};

TEST_F(IOSChromeMetricsServiceAccessorTest, MetricsReportingEnabled) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* pref = metrics::prefs::kMetricsReportingEnabled;
  GetLocalState()->SetDefaultPrefValue(pref, base::Value(false));

  GetLocalState()->SetBoolean(pref, false);
  EXPECT_FALSE(
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->SetBoolean(pref, true);
  EXPECT_TRUE(
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  GetLocalState()->ClearPref(pref);
  EXPECT_FALSE(
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#else
  // Metrics Reporting is never enabled when GOOGLE_CHROME_BRANDING is not set.
  EXPECT_FALSE(
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
#endif
}
