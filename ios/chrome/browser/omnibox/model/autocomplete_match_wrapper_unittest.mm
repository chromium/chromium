// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper.h"

#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper_delegate.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

@interface FakeAutocompleteMatchWrapperDelegate
    : NSObject <AutocompleteMatchWrapperDelegate>

@property(nonatomic, assign) BOOL isStarred;

@end

@implementation FakeAutocompleteMatchWrapperDelegate

- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  return self.isStarred;
}

@end

class AutocompleteMatchWrapperTest : public PlatformTest {
 public:
  AutocompleteMatchWrapperTest() {
    _fake_autocomplete_wrapper_delegate =
        [[FakeAutocompleteMatchWrapperDelegate alloc] init];
    wrapper_ = [[AutocompleteMatchWrapper alloc] init];
    wrapper_.isIncognito = NO;
    wrapper_.templateURLService =
        search_engines_test_environment_.template_url_service();
    wrapper_.delegate = _fake_autocomplete_wrapper_delegate;

    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }

  ~AutocompleteMatchWrapperTest() override {
    [wrapper_ disconnect];
  }

  base::test::TaskEnvironment task_environment_;

  AutocompleteMatchWrapper* wrapper_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeAutocompleteMatchWrapperDelegate* _fake_autocomplete_wrapper_delegate;
};

// Test wrapping a search match.
TEST_F(AutocompleteMatchWrapperTest,
       testWrapMatchesFromResultWithStarredMatch) {
  AutocompleteMatch match = CreateSearchMatch(u"search");

  _fake_autocomplete_wrapper_delegate.isStarred = YES;

  AutocompleteResult result;

  result.AppendMatches({match});

  wrapper_.hasThumbnail = YES;

  NSMutableArray<AutocompleteMatchFormatter*>* wrappedMatches =
      [wrapper_ wrapMatchesFromResult:result];

  EXPECT_EQ(wrappedMatches.count, 1u);

  EXPECT_TRUE(wrappedMatches[0].starred);
  EXPECT_FALSE(wrappedMatches[0].incognito);
  EXPECT_TRUE(wrappedMatches[0].defaultSearchEngineIsGoogle);
  EXPECT_TRUE(wrappedMatches[0].isMultimodal);
  EXPECT_EQ(wrappedMatches[0].actionsInSuggest.count, 0u);

  // Reset isStarred to NO.
  _fake_autocomplete_wrapper_delegate.isStarred = NO;
}

// Test wrapping matches form a given autocomplete result.
TEST_F(AutocompleteMatchWrapperTest, testWrapMatchesFromResult) {
  AutocompleteResult result;

  AutocompleteMatch match1 = CreateActionInSuggestMatch(
      u"Action", {omnibox::ActionInfo_ActionType_REVIEWS,
                  omnibox::ActionInfo_ActionType_DIRECTIONS});
  AutocompleteMatch match2 = CreateSearchMatch(u"search");

  result.AppendMatches({match1, match2});
  wrapper_.hasThumbnail = NO;

  NSMutableArray<AutocompleteMatchFormatter*>* wrappedMatches =
      [wrapper_ wrapMatchesFromResult:result];

  EXPECT_EQ(wrappedMatches.count, 2u);

  EXPECT_FALSE(wrappedMatches[0].starred);
  EXPECT_FALSE(wrappedMatches[1].starred);

  EXPECT_FALSE(wrappedMatches[0].incognito);
  EXPECT_FALSE(wrappedMatches[1].incognito);

  EXPECT_TRUE(wrappedMatches[0].defaultSearchEngineIsGoogle);
  EXPECT_TRUE(wrappedMatches[1].defaultSearchEngineIsGoogle);

  EXPECT_FALSE(wrappedMatches[0].isMultimodal);
  EXPECT_FALSE(wrappedMatches[0].isMultimodal);

  EXPECT_EQ(wrappedMatches[0].actionsInSuggest.count, 2u);
  EXPECT_EQ(wrappedMatches[1].actionsInSuggest.count, 0u);
}

// Test wrapping a search match after changing the default search engine.
TEST_F(AutocompleteMatchWrapperTest, testChangeSearchEngine) {
  AutocompleteResult result;

  AutocompleteMatch match1 = CreateActionInSuggestMatch(
      u"Action", {omnibox::ActionInfo_ActionType_REVIEWS,
                  omnibox::ActionInfo_ActionType_DIRECTIONS});
  AutocompleteMatch match2 = CreateSearchMatch(u"search");

  result.AppendMatches({match1, match2});

  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();

  template_url_service->Load();
  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  NSMutableArray<AutocompleteMatchFormatter*>* wrappedMatches =
      [wrapper_ wrapMatchesFromResult:result];

  EXPECT_EQ(wrappedMatches.count, 2u);

  // the `wrappedMatch` defaultSearchEngineIsGoogle should be true.
  EXPECT_TRUE(wrappedMatches[0].defaultSearchEngineIsGoogle);
  EXPECT_TRUE(wrappedMatches[1].defaultSearchEngineIsGoogle);

  // Keep a reference to the Google default search provider.
  const TemplateURL* google_provider =
      template_url_service->GetDefaultSearchProvider();

  // Change the default search provider to a non-Google one.
  TemplateURLData non_google_provider_data;
  non_google_provider_data.SetURL("https://www.nongoogle.com/?q={searchTerms}");
  non_google_provider_data.suggestions_url =
      "https://www.nongoogle.com/suggest/?q={searchTerms}";
  auto* non_google_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(non_google_provider_data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      non_google_provider);

  wrappedMatches = [wrapper_ wrapMatchesFromResult:result];

  // the `wrappedMatch` defaultSearchEngineIsGoogle should now be false.
  EXPECT_FALSE(wrappedMatches[0].defaultSearchEngineIsGoogle);
  EXPECT_FALSE(wrappedMatches[1].defaultSearchEngineIsGoogle);

  // Change the default search provider back to Google.
  template_url_service->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));
}
