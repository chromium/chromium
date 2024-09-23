// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"

#import "build/branding_buildflags.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class IOSChromeMetricsServiceAccessorTest : public PlatformTest {
 public:
  IOSChromeMetricsServiceAccessorTest() {}

  IOSChromeMetricsServiceAccessorTest(
      const IOSChromeMetricsServiceAccessorTest&) = delete;
  IOSChromeMetricsServiceAccessorTest& operator=(
      const IOSChromeMetricsServiceAccessorTest&) = delete;

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
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
