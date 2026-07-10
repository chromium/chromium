// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

namespace ai {

class ZeroStateSuggestionsServiceTest : public PlatformTest {
 protected:
  ZeroStateSuggestionsServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kZeroStateSuggestions}, {});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    profile_ = std::move(builder).Build();

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(profile_.get());
    fake_web_state_->SetVisibleURL(GURL("https://example.com"));

    service_ =
        std::make_unique<ZeroStateSuggestionsService>(fake_web_state_.get());
  }

  void SetCachedSuggestions(const std::vector<std::string>& suggestions) {
    service_->suggestions_ = suggestions;
    service_->suggestions_url_ = fake_web_state_->GetVisibleURL();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<ZeroStateSuggestionsService> service_;
};

// Tests that populated cache correctly returns suggestions.
TEST_F(ZeroStateSuggestionsServiceTest, TestFetchCachedSuggestions) {
  std::vector<std::string> suggestions = {"suggestion1", "suggestion2"};
  SetCachedSuggestions(suggestions);

  base::test::TestFuture<NSArray<ZeroStateSuggestion*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  NSArray<ZeroStateSuggestion*>* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(2u, result.count);
  EXPECT_NSEQ(@"suggestion1", result[0].text);
  EXPECT_NSEQ(@"suggestion2", result[1].text);
}

// Tests that ClearCachedSuggestions clears the cached suggestions.
TEST_F(ZeroStateSuggestionsServiceTest, TestClearCachedSuggestions) {
  std::vector<std::string> suggestions = {"suggestion1"};
  SetCachedSuggestions(suggestions);

  service_->ClearCachedSuggestions();

  base::test::TestFuture<NSArray<ZeroStateSuggestion*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_NSEQ(nil, future.Get());
}

// Tests that populated cache correctly returns suggestions.
TEST_F(ZeroStateSuggestionsServiceTest, TestFetchCentralizedCachedSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kZeroStateSuggestionsCentralization);

  std::vector<std::string> suggestions = {"suggestion1", "suggestion2"};
  SetCachedSuggestions(suggestions);

  base::test::TestFuture<NSArray<ZeroStateSuggestion*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  NSArray<ZeroStateSuggestion*>* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(3u, result.count);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_TEXT),
      result[0].text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_QUERY),
      result[0].query);
  EXPECT_NSEQ(nil, result[0].iconIdentifier);

  EXPECT_NSEQ(@"suggestion1", result[1].text);
  EXPECT_NSEQ(@"suggestion1", result[1].query);
  EXPECT_NSEQ(nil, result[1].iconIdentifier);

  EXPECT_NSEQ(@"suggestion2", result[2].text);
  EXPECT_NSEQ(@"suggestion2", result[2].query);
  EXPECT_NSEQ(nil, result[2].iconIdentifier);
}

// Tests that suggestions return static fallbacks when model suggestions are
// empty.
TEST_F(ZeroStateSuggestionsServiceTest,
       TestFetchCentralizedSuggestionsEmptyModelSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kZeroStateSuggestionsCentralization, kZeroStateSuggestionsWCGD}, {});

  EXPECT_CALL(*mock_tracker_,
              WouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiWhatCanGeminiDo)))
      .WillRepeatedly(testing::Return(true));

  std::vector<std::string> suggestions = {};
  SetCachedSuggestions(suggestions);

  base::test::TestFuture<NSArray<ZeroStateSuggestion*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  NSArray<ZeroStateSuggestion*>* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(3u, result.count);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_TEXT),
      result[0].text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_QUERY),
      result[0].query);
  EXPECT_NSEQ(nil, result[0].iconIdentifier);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_FAQ_TEXT),
              result[1].text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_FAQ_QUERY),
              result[1].query);
  EXPECT_NSEQ(nil, result[1].iconIdentifier);

  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_ZERO_STATE_SUGGESTIONS_WHAT_CAN_GEMINI_DO_TEXT),
              result[2].text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_ZERO_STATE_SUGGESTIONS_WHAT_CAN_GEMINI_DO_QUERY),
              result[2].query);
  EXPECT_NSEQ(nil, result[2].iconIdentifier);
}

}  // namespace ai
