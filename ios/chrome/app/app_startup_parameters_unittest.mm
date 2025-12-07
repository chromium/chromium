// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/providers/application_mode_fetcher/test_application_mode_fetcher.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

const char kIncognitoModeUrl[] = "http://www.incognito.com";

const char kErrorUrl[] = "http://www.error.com";

const char kTimeOutErrorUrl[] = "http://www.timeOutError.com";

NSString* youtubeAppID = @"com.google.ios.youtube";

}  // namespace

// An ApplicationModeFetcher factory that sends back a fetching response
// depending on the url, it will also store a `mode`property to read and write.
@interface TestApplicationModeFetcherProviderTestHelper
    : NSObject <ApplicationModeFetcherProviderTestHelper>

- (void)setMode:(ApplicationModeForTabOpening)mode;
- (ApplicationModeForTabOpening)mode;

@end

@implementation TestApplicationModeFetcherProviderTestHelper {
  ApplicationModeForTabOpening _mode;
}

- (void)sendFetchingResponseForUrl:(const GURL&)url
                        completion:(FetchingResponseCompletion)completion {
  if (!completion) {
    return;
  }

  if (url == GURL(kIncognitoModeUrl)) {
    completion(true, nil);
    return;
  }

  if (url == GURL(kErrorUrl)) {
    completion(false, [NSError errorWithDomain:@"FetchingError"
                                          code:1
                                      userInfo:nil]);
    return;
  }

  if (url == GURL(kTimeOutErrorUrl)) {
    completion(false, [NSError errorWithDomain:@"AppSwitcherTimeoutError"
                                          code:1
                                      userInfo:nil]);
    return;
  }

  completion(false, nil);
}

- (void)setMode:(ApplicationModeForTabOpening)mode {
  _mode = mode;
}

- (ApplicationModeForTabOpening)mode {
  return _mode;
}

@end

// Test the `AppStartupParameters` and its fetched application mode.
class AppStartupParamsTest : public PlatformTest {
 public:
  void SetUp() override {
    helper_ = [[TestApplicationModeFetcherProviderTestHelper alloc] init];
    ios::provider::test::SetApplicationModeFetcherProviderTestHelper(helper_);
    feature_list_.InitAndEnableFeature(kYoutubeIncognito);
  }

  void TearDown() override {
    ios::provider::test::SetApplicationModeFetcherProviderTestHelper(nil);
    PlatformTest::TearDown();
  }

 protected:
  TestApplicationModeFetcherProviderTestHelper* helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the requested mode is incognito when the url's source app is
// allowed and the response is incognito.
TEST_F(AppStartupParamsTest,
       TestSucessIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL incognito_url = GURL(kIncognitoModeUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:incognito_url
               completeURL:incognito_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode],
            ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO);
  histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 0, 1);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
}

// Tests that the app mode is `APP_SWITCHER_UNDETERMINED` when an error is
// raised by the Application Mode Fetcher provider when the url's source app is
// allowed.
TEST_F(AppStartupParamsTest,
       TestFailedIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL error_url = GURL(kErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:error_url
               completeURL:error_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode],
            ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED);
  histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 2, 1);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
}

// Tests that the requested mode is unchanged when the url's source app is
// allowed and the response is non incognito.
TEST_F(AppStartupParamsTest, TestNonIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL url = GURL("");
  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:url
               completeURL:url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode], ApplicationModeForTabOpening::NORMAL);
  histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 1, 1);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
}

// Tests that the app mode is `APP_SWITCHER_UNDETERMINED` when an error is
// raised by the Application Mode Fetcher provider when the url's source app is
// allowed. And the metric is properly recorded when the error is a time out.
TEST_F(AppStartupParamsTest, TestAppModeRequestTimeOutForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL timeout_url = GURL(kTimeOutErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:timeout_url
               completeURL:timeout_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode],
            ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED);
  histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 3, 1);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
}

// Tests that if the app id is not allowed the response is always non incognito
// for an eligible URL.
TEST_F(AppStartupParamsTest,
       TestSucessIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL incognito_url = GURL(kIncognitoModeUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:incognito_url
               completeURL:incognito_url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode], ApplicationModeForTabOpening::NORMAL);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
}

// Tests that if the app id is not allowed the response is always non incognito
// for an error generating URL.
TEST_F(AppStartupParamsTest,
       TestFailedIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL error_url = GURL(kErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:error_url
               completeURL:error_url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode], ApplicationModeForTabOpening::NORMAL);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
}

// Tests that if the app id is not allowed the response is always non incognito
// for a given URL.
TEST_F(AppStartupParamsTest, TestNonIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL url = GURL("");
  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:url
               completeURL:url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];
  [params requestApplicationModeWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([helper_ mode], ApplicationModeForTabOpening::NORMAL);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
}
