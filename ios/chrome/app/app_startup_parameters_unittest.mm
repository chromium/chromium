// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/app_switcher/metrics/app_switcher_metrics.h"
#import "ios/chrome/browser/app_switcher/test/test_app_switcher_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/providers/app_switcher/test_app_switcher.h"
#import "ios/chrome/test/providers/application_mode_fetcher/test_application_mode_fetcher.h"
#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

NSString* youtubeAppID = @"com.google.ios.youtube";
NSString* gmailAppID = @"com.google.Gmail";

}  // namespace

// TODO(crbug.com/465336545): Remove this class once the feature is fully
// launched. Helper conforming to ApplicationModeFetcherProviderTestHelper
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

// Test the `AppStartupParameters` and its fetched application mode /
// parameters.
class AppStartupParamsTest : public PlatformTest,
                             public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{kAppSwitcherAISummarization, kPageActionMenu},
          /*disabled_features=*/{});
      app_switcher_helper_ = [[TestAppSwitcherProviderTestHelper alloc] init];
      ios::provider::test::SetAppSwitcherProviderTestHelper(
          app_switcher_helper_);
    } else {
      feature_list_.InitAndDisableFeature(kAppSwitcherAISummarization);
      helper_ = [[TestApplicationModeFetcherProviderTestHelper alloc] init];
      ios::provider::test::SetApplicationModeFetcherProviderTestHelper(helper_);
    }
  }

  void TearDown() override {
    if (GetParam()) {
      ios::provider::test::SetAppSwitcherProviderTestHelper(nil);
    } else {
      ios::provider::test::SetApplicationModeFetcherProviderTestHelper(nil);
    }
    PlatformTest::TearDown();
  }

  // Triggers parameter fetching on `params` using the appropriate provider API
  // based on whether `kAppSwitcherAISummarization` is enabled.
  void FetchParams(AppStartupParameters* params) {
    [params fetchAppSwitcherParamsWithBlock:^(
                ApplicationModeForTabOpening applicationMode) {
      if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
        [app_switcher_helper_ setMode:applicationMode];
      } else {
        [helper_ setMode:applicationMode];
      }
    }];
  }

  // Returns the application mode recorded by the active test helper.
  ApplicationModeForTabOpening GetFetchedMode() {
    return base::FeatureList::IsEnabled(kAppSwitcherAISummarization)
               ? [app_switcher_helper_ mode]
               : [helper_ mode];
  }

 protected:
  TestAppSwitcherProviderTestHelper* app_switcher_helper_;
  // TODO(crbug.com/465336545): Refactor this and related unit tests to use
  // `app_switcher_helper_` instead.
  TestApplicationModeFetcherProviderTestHelper* helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the requested mode is incognito when the url's source
// app is allowed and the response is incognito.
TEST_P(AppStartupParamsTest,
       TestSuccessIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL incognito_url = GURL(kIncognitoModeUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:incognito_url
               completeURL:incognito_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(),
            ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", true, 1);
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
  } else {
    histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 0, 1);
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
  }
}

// Tests that the app mode is `APP_SWITCHER_UNDETERMINED` when an
// error is raised by the provider when the url's source app is allowed.
TEST_P(AppStartupParamsTest,
       TestFailedIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL error_url = GURL(kErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:error_url
               completeURL:error_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(),
            ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", false,
                                       1);
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
  } else {
    histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 2, 1);
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
  }
}

// Tests that the requested mode is unchanged when the url's source
// app is allowed and the response is non incognito.
TEST_P(AppStartupParamsTest, TestNonIncognitoModeRequestForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL url = GURL("");
  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:url
               completeURL:url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(), ApplicationModeForTabOpening::NORMAL);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", true, 1);
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
  } else {
    histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 1, 1);
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
  }
}

// Tests that the app mode is `APP_SWITCHER_UNDETERMINED` when an
// error is raised by the provider when the url's source app is allowed.
// And the metric is properly recorded when the error is a time out.
TEST_P(AppStartupParamsTest, TestAppModeRequestTimeOutForAllowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL timeout_url = GURL(kTimeOutErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:timeout_url
               completeURL:timeout_url
               sourceAppID:youtubeAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(),
            ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", false,
                                       1);
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
  } else {
    histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", 3, 1);
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 1);
  }
}

// Tests that if the app id is not allowed the response is always non
// incognito for an eligible URL.
TEST_P(AppStartupParamsTest,
       TestSuccessIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL incognito_url = GURL(kIncognitoModeUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:incognito_url
               completeURL:incognito_url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(), ApplicationModeForTabOpening::NORMAL);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 0);
  } else {
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
  }
}

// Tests that if the app id is not allowed the response is always non
// incognito for an error generating URL.
TEST_P(AppStartupParamsTest,
       TestFailedIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL error_url = GURL(kErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:error_url
               completeURL:error_url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(), ApplicationModeForTabOpening::NORMAL);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 0);
  } else {
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
  }
}

// Tests that if the app id is not allowed the response is always non
// incognito for a given URL.
TEST_P(AppStartupParamsTest, TestNonIncognitoModeRequestForUnallowedSourceApp) {
  base::HistogramTester histogram_tester;
  GURL url = GURL("");

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:url
               completeURL:url
               sourceAppID:nil
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  FetchParams(params);

  EXPECT_EQ(GetFetchedMode(), ApplicationModeForTabOpening::NORMAL);

  if (base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 0);
  } else {
    histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
  }
}

// Tests that the post opening action is set to `START_GEMINI_AI_SUMMARIZATION`
// when the url's source app is allowed for AI summarization and the response
// is AI summarization.
TEST_P(AppStartupParamsTest,
       TestSuccessAISummarizationRequestForAllowedSourceApp) {
  if (!base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    GTEST_SKIP() << "AI Summarization is only available when App Switcher "
                    "AISummarization is enabled.";
  }

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  GURL summarize_url = GURL(kAISummarizationUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:summarize_url
               completeURL:summarize_url
               sourceAppID:gmailAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  EXPECT_EQ([params postOpeningAction], TabOpeningPostOpeningAction::NO_ACTION);

  [params fetchAppSwitcherParamsWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [app_switcher_helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([params postOpeningAction],
            TabOpeningPostOpeningAction::START_GEMINI_AI_SUMMARIZATION);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.AISummarization.AppSwitcherEntrypoint"));
  histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", true, 1);
  histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
}

// Tests that the post opening action is unchanged when there's an error in the
// response for an AI summarization allowed source app.
TEST_P(AppStartupParamsTest,
       TestFailedAISummarizationRequestForAllowedSourceApp) {
  if (!base::FeatureList::IsEnabled(kAppSwitcherAISummarization)) {
    GTEST_SKIP() << "AI Summarization is only available when App Switcher "
                    "AISummarization is enabled.";
  }

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  GURL error_url = GURL(kErrorUrl);

  AppStartupParameters* params = [[AppStartupParameters alloc]
       initWithExternalURL:error_url
               completeURL:error_url
               sourceAppID:gmailAppID
           applicationMode:ApplicationModeForTabOpening::NORMAL
      forceApplicationMode:NO];

  EXPECT_EQ([params postOpeningAction], TabOpeningPostOpeningAction::NO_ACTION);

  [params fetchAppSwitcherParamsWithBlock:^(
              ApplicationModeForTabOpening applicationMode) {
    [app_switcher_helper_ setMode:applicationMode];
  }];

  EXPECT_EQ([params postOpeningAction], TabOpeningPostOpeningAction::NO_ACTION);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "IOS.AISummarization.AppSwitcherEntrypoint"));
  histogram_tester.ExpectBucketCount("IOS.AppSwitcher.FetchOutcome", false, 1);
  histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 1);
}

INSTANTIATE_TEST_SUITE_P(, AppStartupParamsTest, testing::Bool());
