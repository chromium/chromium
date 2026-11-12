// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_switcher_params.h"

#import <optional>
#import <string_view>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/app_switcher/test/test_app_switcher_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/providers/app_switcher/test_app_switcher.h"
#import "ios/chrome/test/providers/application_mode_fetcher/test_application_mode_fetcher.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Caller app allowlisted for application mode only.
NSString* const kYoutubeAppID = @"com.google.ios.youtube";
// Caller app allowlisted for AI summarization only.
NSString* const kGmailAppID = @"com.google.Gmail";
// Caller app allowlisted for neither application mode nor AI summarization.
NSString* const kOtherAppID = @"com.example.app";

// Error domain reported by the application mode fetcher on time out.
NSString* const kTimeOutErrorDomain = @"AppSwitcherTimeoutError";
// Error domain of a generic fetch failure.
NSString* const kFetchingErrorDomain = @"FetchingError";

// `IOS.AppModeFetching.Outcome` buckets.
constexpr int kOutcomeIncognito = 0;
constexpr int kOutcomeNonIncognito = 1;
constexpr int kOutcomeFailure = 2;
constexpr int kOutcomeTimeOut = 3;

// Returns an error with `domain`.
NSError* ErrorWithDomain(NSString* domain) {
  return [NSError errorWithDomain:domain code:1 userInfo:nil];
}

}  // namespace

// Application mode fetcher test helper replying with a configurable response.
@interface FakeApplicationModeFetcherHelper
    : NSObject <ApplicationModeFetcherProviderTestHelper>

// Whether the response is incognito.
@property(nonatomic, assign) BOOL isIncognito;
// The error of the response.
@property(nonatomic, strong) NSError* error;

@end

@implementation FakeApplicationModeFetcherHelper

- (void)sendFetchingResponseForUrl:(const GURL&)url
                        completion:(FetchingResponseCompletion)completion {
  completion(self.isIncognito, self.error);
}

@end

// Tests the App Switcher parameters helpers.
class AppSwitcherParamsTest : public PlatformTest {
 protected:
  void TearDown() override {
    ios::provider::test::SetAppSwitcherProviderTestHelper(nil);
    ios::provider::test::SetApplicationModeFetcherProviderTestHelper(nil);
    PlatformTest::TearDown();
  }

  // Enables AI summarization and installs the App Switcher test helper.
  void EnableAISummarization() {
    feature_list_.InitWithFeatures(
        {kAppSwitcherAISummarization, kPageActionMenu}, {});
    app_switcher_helper_ = [[TestAppSwitcherProviderTestHelper alloc] init];
    ios::provider::test::SetAppSwitcherProviderTestHelper(app_switcher_helper_);
  }

  // Disables AI summarization and installs the application mode fetcher test
  // helper.
  void DisableAISummarization() {
    feature_list_.InitAndDisableFeature(kAppSwitcherAISummarization);
    mode_fetcher_helper_ = [[FakeApplicationModeFetcherHelper alloc] init];
    ios::provider::test::SetApplicationModeFetcherProviderTestHelper(
        mode_fetcher_helper_);
  }

  // Requests the App Switcher parameters for `url` opened from
  // `source_app_id`, and returns the result. The test helpers reply
  // synchronously.
  AppSwitcherParams Request(std::string_view url, NSString* source_app_id) {
    std::optional<AppSwitcherParams> result;
    RequestAppSwitcherParams(
        GURL(url), source_app_id,
        base::BindOnce(
            [](std::optional<AppSwitcherParams>* result,
               const AppSwitcherParams& params) { *result = params; },
            &result));
    CHECK(result.has_value());
    return *result;
  }

  base::test::ScopedFeatureList feature_list_;
  TestAppSwitcherProviderTestHelper* app_switcher_helper_;
  FakeApplicationModeFetcherHelper* mode_fetcher_helper_;
};

// Tests that, with AI summarization enabled, parameters are fetched for AI
// summarization callers, and for application mode callers unless incognito is
// forced.
TEST_F(AppSwitcherParamsTest, TestShouldFetchWithAISummarization) {
  EnableAISummarization();
  EXPECT_TRUE(ShouldFetchAppSwitcherParams(kYoutubeAppID, false));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(kYoutubeAppID, true));
  EXPECT_TRUE(ShouldFetchAppSwitcherParams(kGmailAppID, false));
  EXPECT_TRUE(ShouldFetchAppSwitcherParams(kGmailAppID, true));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(kOtherAppID, false));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(nil, false));
}

// Tests that, with AI summarization disabled, parameters are only fetched for
// application mode callers when incognito is not forced.
TEST_F(AppSwitcherParamsTest, TestShouldFetchWithoutAISummarization) {
  DisableAISummarization();
  EXPECT_TRUE(ShouldFetchAppSwitcherParams(kYoutubeAppID, false));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(kYoutubeAppID, true));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(kGmailAppID, false));
  EXPECT_FALSE(ShouldFetchAppSwitcherParams(kOtherAppID, false));
}

// Tests that, with AI summarization disabled, the application mode fetcher
// response is returned and its metrics are deferred to
// `RecordAppModeFetchingMetrics()`.
TEST_F(AppSwitcherParamsTest, TestRequestWithoutAISummarization) {
  DisableAISummarization();
  mode_fetcher_helper_.isIncognito = YES;
  base::HistogramTester histogram_tester;

  AppSwitcherParams params = Request(kIncognitoModeUrl, kYoutubeAppID);

  EXPECT_TRUE(params.can_change_application_mode);
  EXPECT_TRUE(params.is_incognito);
  EXPECT_FALSE(params.error);
  EXPECT_FALSE(params.is_ai_summarization);
  EXPECT_TRUE(params.should_record_app_mode_fetching);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
  histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchOutcome", 0);
}

// Tests that an incognito App Switcher response for an application mode caller
// can change the mode and records the App Switcher fetch metrics.
TEST_F(AppSwitcherParamsTest, TestRequestIncognitoForApplicationModeCaller) {
  EnableAISummarization();
  base::HistogramTester histogram_tester;

  AppSwitcherParams params = Request(kIncognitoModeUrl, kYoutubeAppID);

  EXPECT_TRUE(params.can_change_application_mode);
  EXPECT_TRUE(params.is_incognito);
  EXPECT_FALSE(params.is_ai_summarization);
  EXPECT_FALSE(params.should_record_app_mode_fetching);
  histogram_tester.ExpectUniqueSample("IOS.AppSwitcher.FetchOutcome", true, 1);
  histogram_tester.ExpectTotalCount("IOS.AppSwitcher.FetchDuration", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.AppSwitcher.AISummarization.FetchOutcome", 0);
}

// Tests that an AI summarization response for an AI summarization caller
// returns the hashed user ID and records the AI summarization metrics, without
// allowing the caller to change the mode.
TEST_F(AppSwitcherParamsTest, TestRequestAISummarizationForAICaller) {
  EnableAISummarization();
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  AppSwitcherParams params = Request(kAISummarizationUrl, kGmailAppID);

  EXPECT_TRUE(params.is_ai_summarization);
  EXPECT_NSEQ(@"test_user_hash_12345", params.hashed_user_id);
  EXPECT_FALSE(params.can_change_application_mode);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.AISummarization.AppSwitcherEntrypoint"));
  histogram_tester.ExpectUniqueSample("IOS.AppSwitcher.FetchOutcome", true, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.AppSwitcher.AISummarization.FetchOutcome", true, 1);
}

// Tests that an incognito response is ignored for a caller not allowlisted for
// application mode.
TEST_F(AppSwitcherParamsTest, TestRequestIncognitoForAICaller) {
  EnableAISummarization();

  AppSwitcherParams params = Request(kIncognitoModeUrl, kGmailAppID);

  EXPECT_FALSE(params.can_change_application_mode);
  EXPECT_FALSE(params.is_incognito);
  EXPECT_FALSE(params.is_ai_summarization);
}

// Tests that an AI summarization response is ignored for a caller not
// allowlisted for AI summarization.
TEST_F(AppSwitcherParamsTest,
       TestRequestAISummarizationForApplicationModeCaller) {
  EnableAISummarization();
  base::UserActionTester user_action_tester;

  AppSwitcherParams params = Request(kAISummarizationUrl, kYoutubeAppID);

  EXPECT_FALSE(params.is_ai_summarization);
  EXPECT_FALSE(params.hashed_user_id);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "IOS.AISummarization.AppSwitcherEntrypoint"));
}

// Tests that a failed App Switcher fetch records a failed outcome.
TEST_F(AppSwitcherParamsTest, TestRequestError) {
  EnableAISummarization();
  base::HistogramTester histogram_tester;

  AppSwitcherParams params = Request(kErrorUrl, kYoutubeAppID);

  EXPECT_TRUE(params.error);
  histogram_tester.ExpectUniqueSample("IOS.AppSwitcher.FetchOutcome", false, 1);
}

// Tests that the application mode is resolved from the fetched parameters.
TEST_F(AppSwitcherParamsTest, TestApplicationModeForAppSwitcherParams) {
  constexpr ApplicationModeForTabOpening kCurrentMode =
      ApplicationModeForTabOpening::NORMAL;
  AppSwitcherParams params;
  params.is_incognito = true;
  EXPECT_EQ(kCurrentMode,
            ApplicationModeForAppSwitcherParams(params, kCurrentMode));

  params.can_change_application_mode = true;
  EXPECT_EQ(ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO,
            ApplicationModeForAppSwitcherParams(params, kCurrentMode));

  params.is_incognito = false;
  params.error = ErrorWithDomain(kFetchingErrorDomain);
  EXPECT_EQ(ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED,
            ApplicationModeForAppSwitcherParams(params, kCurrentMode));

  params.error = nil;
  EXPECT_EQ(kCurrentMode,
            ApplicationModeForAppSwitcherParams(params, kCurrentMode));
}

// Tests that the application mode fetching metrics are only recorded for
// parameters from the application mode fetcher.
TEST_F(AppSwitcherParamsTest, TestRecordAppModeFetchingMetricsSkipped) {
  base::HistogramTester histogram_tester;
  AppSwitcherParams params;
  params.is_incognito = true;

  RecordAppModeFetchingMetrics(params);

  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Outcome", 0);
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Duration", 0);
}

// Tests that each application mode fetching outcome bucket is recorded,
// including time out.
TEST_F(AppSwitcherParamsTest, TestRecordAppModeFetchingMetrics) {
  base::HistogramTester histogram_tester;
  AppSwitcherParams params;
  params.should_record_app_mode_fetching = true;
  params.fetch_start_time = base::TimeTicks::Now();

  params.is_incognito = true;
  RecordAppModeFetchingMetrics(params);
  params.is_incognito = false;
  RecordAppModeFetchingMetrics(params);
  params.error = ErrorWithDomain(kFetchingErrorDomain);
  RecordAppModeFetchingMetrics(params);
  params.error = ErrorWithDomain(kTimeOutErrorDomain);
  RecordAppModeFetchingMetrics(params);

  for (int bucket : {kOutcomeIncognito, kOutcomeNonIncognito, kOutcomeFailure,
                     kOutcomeTimeOut}) {
    SCOPED_TRACE(bucket);
    histogram_tester.ExpectBucketCount("IOS.AppModeFetching.Outcome", bucket,
                                       1);
  }
  histogram_tester.ExpectTotalCount("IOS.AppModeFetching.Duration", 4);
}
