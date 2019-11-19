// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/firebase_utils.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/app/firebase_buildflags.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#if BUILDFLAG(FIREBASE_ENABLED)
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRApp.h"
#endif  // BUILDFLAG(FIREBASE_ENABLED)
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/test_app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if BUILDFLAG(FIREBASE_ENABLED)
// This always returns positively that a user is a legacy user (i.e. installed
// Chrome before Chrome supports Firebase).
class LegacyUserAppDistributionProvider : public TestAppDistributionProvider {
 public:
  bool IsPreFirebaseLegacyUser(int64_t install_date) override { return true; }
};

// LegacyUserChromeBrowserProvider is a fake ChromeBrowserProvider that
// always returns positively that user is a legacy user. This is intended for
// testing the case of legacy users.
// NOTE: The default TestChromeBrowserProvider for unit tests always treats
// a user as a new user.
class LegacyUserChromeBrowserProvider : public ios::TestChromeBrowserProvider {
 public:
  LegacyUserChromeBrowserProvider()
      : app_distribution_provider_(
            std::make_unique<LegacyUserAppDistributionProvider>()) {}
  ~LegacyUserChromeBrowserProvider() override {}

  AppDistributionProvider* GetAppDistributionProvider() const override {
    return app_distribution_provider_.get();
  }

 private:
  std::unique_ptr<LegacyUserAppDistributionProvider> app_distribution_provider_;
  DISALLOW_COPY_AND_ASSIGN(LegacyUserChromeBrowserProvider);
};
#endif  // BUILDFLAG(FIREBASE_ENABLED)

class FirebaseUtilsTest : public PlatformTest {
 public:
  FirebaseUtilsTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
  }
  void SetUp() override {
    PlatformTest::SetUp();
#if BUILDFLAG(FIREBASE_ENABLED)
    firapp_ = OCMClassMock([FIRApp class]);
#endif  // BUILDFLAG(FIREBASE_ENABLED)
    SetInstallDate(base::Time::Now().ToTimeT());
  }
  void TearDown() override {
    [firapp_ stopMocking];
    PlatformTest::TearDown();
  }
  void SetInstallDate(int64_t install_date) {
    GetApplicationContext()->GetLocalState()->SetInt64(
        metrics::prefs::kInstallDate, install_date);
  }

 protected:
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::WebTaskEnvironment task_environment_;
  id firapp_;
  base::HistogramTester histogram_tester_;
};

#if BUILDFLAG(FIREBASE_ENABLED)

TEST_F(FirebaseUtilsTest, EnabledInitializedHistogramFirstRun) {
  // Expects Firebase SDK initialization to be called.
  [[firapp_ expect] configure];
  InitializeFirebase(/*is_first_run=*/true);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kEnabledFirstRun, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

TEST_F(FirebaseUtilsTest, EnabledInitializedHistogramNotFirstRun) {
  // Expects Firebase SDK initialization to be called.
  [[firapp_ expect] configure];
  InitializeFirebase(/*is_first_run=*/false);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kEnabledNotFirstRun, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

TEST_F(FirebaseUtilsTest, DisabledInitializedOutsideWindow) {
  // Set an app install date that is older than the conversion attribution
  // window.
  int64_t before_attribution =
      base::TimeDelta::FromDays(kConversionAttributionWindowInDays + 1)
          .InSeconds();
  SetInstallDate(base::Time::Now().ToTimeT() - before_attribution);
  // Expects Firebase SDK initialization to be not called.
  [[firapp_ reject] configure];
  InitializeFirebase(/*is_first_run=*/false);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kDisabledConversionWindow, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

TEST_F(FirebaseUtilsTest, DisabledLegacyInstallation) {
  // Sets up the ChromeProviderEnvironment with a fake provider that responds
  // positively that user is a legacy user predating Firebase integration.
  // Installation date is irrelevant in this case.
  IOSChromeScopedTestingChromeBrowserProvider provider(
      std::make_unique<LegacyUserChromeBrowserProvider>());
  // Expects Firebase SDK initialization to be not called.
  [[firapp_ reject] configure];
  InitializeFirebase(/*is_first_run=*/true);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kDisabledLegacyInstallation, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

#else

TEST_F(FirebaseUtilsTest, DisabledInitializedHistogram) {
  // FIRApp class should not exist if Firebase is not enabled.
  ASSERT_EQ(nil, NSClassFromString(@"FIRApp"));
  InitializeFirebase(/*is_first_run=*/false);
  histogram_tester_.ExpectUniqueSample(kFirebaseConfiguredHistogramName,
                                       FirebaseConfiguredState::kDisabled, 1);
}

#endif  // BUILDFLAG(FIREBASE_ENABLED)
