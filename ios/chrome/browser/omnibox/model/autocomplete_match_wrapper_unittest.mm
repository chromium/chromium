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
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class AutocompleteMatchWrapperTest : public PlatformTest {
 public:
  AutocompleteMatchWrapperTest() {
    wrapper_ = [[AutocompleteMatchWrapper alloc] init];
    wrapper_.isIncognito = NO;
    wrapper_.templateURLService =
        search_engines_test_environment_.template_url_service();

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
};

// Test wrapping a search match.
TEST_F(AutocompleteMatchWrapperTest, testWrappMatch) {
  AutocompleteMatch match = CreateSearchMatch(u"search");

  AutocompleteResult result;

  result.AppendMatches({match});

  wrapper_.hasThumbnail = YES;

  AutocompleteMatchFormatter* wrappedMatch = [wrapper_ wrapMatch:match
                                                      fromResult:result
                                                       isStarred:YES];

  EXPECT_TRUE(wrappedMatch.starred);
  EXPECT_FALSE(wrappedMatch.incognito);
  EXPECT_TRUE(wrappedMatch.defaultSearchEngineIsGoogle);
  EXPECT_TRUE(wrappedMatch.isMultimodal);
  EXPECT_EQ(wrappedMatch.actionsInSuggest.count, 0u);
}

// Test wrapping an actions in suggest match.
TEST_F(AutocompleteMatchWrapperTest, testWrappMatchWithActions) {
  AutocompleteMatch match = CreateActionInSuggestMatch(
      u"Action", {omnibox::ActionInfo_ActionType_REVIEWS,
                  omnibox::ActionInfo_ActionType_DIRECTIONS});

  AutocompleteResult result;

  result.AppendMatches({match});

  wrapper_.hasThumbnail = NO;

  AutocompleteMatchFormatter* wrappedMatch = [wrapper_ wrapMatch:match
                                                      fromResult:result
                                                       isStarred:YES];

  EXPECT_TRUE(wrappedMatch.starred);
  EXPECT_FALSE(wrappedMatch.incognito);
  EXPECT_FALSE(wrappedMatch.isMultimodal);
  EXPECT_TRUE(wrappedMatch.defaultSearchEngineIsGoogle);
  EXPECT_EQ(wrappedMatch.actionsInSuggest.count, 2u);
}

// Test wrapping a search match after changing the default search engine.
TEST_F(AutocompleteMatchWrapperTest, testChangeSearchEngine) {
  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();

  template_url_service->Load();
  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  AutocompleteMatch match = CreateSearchMatch(u"search");

  AutocompleteResult result;

  result.AppendMatches({match});

  wrapper_.hasThumbnail = YES;

  AutocompleteMatchFormatter* wrappedMatch = [wrapper_ wrapMatch:match
                                                      fromResult:result
                                                       isStarred:YES];

  // the `wrappedMatch` defaultSearchEngineIsGoogle should be true.
  EXPECT_TRUE(wrappedMatch.defaultSearchEngineIsGoogle);

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

  wrappedMatch = [wrapper_ wrapMatch:match fromResult:result isStarred:YES];

  // the `wrappedMatch` defaultSearchEngineIsGoogle should now be false.
  EXPECT_FALSE(wrappedMatch.defaultSearchEngineIsGoogle);

  // Change the default search provider back to Google.
  template_url_service->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));
}
