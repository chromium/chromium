// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper.h"

#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper_delegate.h"
#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/model/suggestions/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

@interface FakeAutocompleteResultWrapperDelegate
    : NSObject <AutocompleteResultWrapperDelegate>

@end

@implementation FakeAutocompleteResultWrapperDelegate

- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups {
  // NO-OP
}

@end

class AutocompleteResultWrapperTest : public PlatformTest {
 public:
  AutocompleteResultWrapperTest() {
    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    _fake_autocomplete_wrapper_delegate =
        [[FakeAutocompleteResultWrapperDelegate alloc] init];
    omnibox_client_ = std::make_unique<TestOmniboxClient>();
    autocomplete_provider_client_ =
        std::make_unique<AutocompleteProviderClientImpl>(profile_.get());

    wrapper_ = [[AutocompleteResultWrapper alloc]
             initWithOmniboxClient:omnibox_client_.get()
        autocompleteProviderClient:autocomplete_provider_client_.get()];
    wrapper_.incognito = NO;
    wrapper_.templateURLService =
        search_engines_test_environment_.template_url_service();
    wrapper_.delegate = _fake_autocomplete_wrapper_delegate;
    wrapper_.pedalAnnotator = [[OmniboxPedalAnnotator alloc] init];
  }

  ~AutocompleteResultWrapperTest() override { [wrapper_ disconnect]; }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  AutocompleteResultWrapper* wrapper_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeAutocompleteResultWrapperDelegate* _fake_autocomplete_wrapper_delegate;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<AutocompleteProviderClientImpl> autocomplete_provider_client_;
};

// Tests wrapping an autocomplete result with 2 non-pedal starred matches.
TEST_F(AutocompleteResultWrapperTest,
       testWrapMatchesFromResultWithStarredMatch) {
  AutocompleteMatch match1 = CreateActionInSuggestMatch(
      u"Action",
      {omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS,
       omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS});
  AutocompleteMatch match2 = CreateSearchMatch(u"search");

  AutocompleteResult result;
  result.AppendMatches({match1, match2});

  wrapper_.hasThumbnail = NO;

  NSArray<id<AutocompleteSuggestionGroup>>* wrappedGroups =
      [wrapper_ wrapAutocompleteResultInGroups:result];

  // Expect 1 wrapped group.
  EXPECT_EQ(wrappedGroups.count, 1u);

  EXPECT_EQ(wrappedGroups[0].type,
            SuggestionGroupType::kUnspecifiedSuggestionGroup);

  // expect 2 wrapped suggestions in the group.
  EXPECT_EQ(wrappedGroups[0].suggestions.count, 2u);

  // Wrapped suggestions should be non pedals.
  EXPECT_TRUE([wrappedGroups[0].suggestions[0]
      isKindOfClass:[AutocompleteMatchFormatter class]]);
  EXPECT_TRUE([wrappedGroups[0].suggestions[1]
      isKindOfClass:[AutocompleteMatchFormatter class]]);

  AutocompleteMatchFormatter* firstSuggestion = wrappedGroups[0].suggestions[0];
  AutocompleteMatchFormatter* secondSuggestion =
      wrappedGroups[0].suggestions[1];

  EXPECT_FALSE(firstSuggestion.starred);
  EXPECT_FALSE(secondSuggestion.starred);

  EXPECT_FALSE(firstSuggestion.incognito);
  EXPECT_FALSE(secondSuggestion.incognito);

  EXPECT_TRUE(firstSuggestion.defaultSearchEngineIsGoogle);
  EXPECT_TRUE(secondSuggestion.defaultSearchEngineIsGoogle);

  EXPECT_FALSE(firstSuggestion.isMultimodal);
  EXPECT_FALSE(secondSuggestion.isMultimodal);

  EXPECT_EQ(firstSuggestion.actionsInSuggest.count, 2u);
  EXPECT_EQ(secondSuggestion.actionsInSuggest.count, 0u);
}

// Tests wrapping an autocomplete result after changing the default search
// engine.
TEST_F(AutocompleteResultWrapperTest, testChangeSearchEngine) {
  AutocompleteResult result;

  AutocompleteMatch match1 = CreateActionInSuggestMatch(
      u"Action",
      {omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS,
       omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS});
  AutocompleteMatch match2 = CreateSearchMatch(u"search");

  result.AppendMatches({match1, match2});

  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();

  template_url_service->Load();
  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  NSArray<id<AutocompleteSuggestionGroup>>* wrappedGroups =
      [wrapper_ wrapAutocompleteResultInGroups:result];

  EXPECT_EQ(wrappedGroups.count, 1u);
  EXPECT_EQ(wrappedGroups[0].suggestions.count, 2u);

  // Wrapped suggestions should be non pedals.
  EXPECT_TRUE([wrappedGroups[0].suggestions[0]
      isKindOfClass:[AutocompleteMatchFormatter class]]);
  EXPECT_TRUE([wrappedGroups[0].suggestions[1]
      isKindOfClass:[AutocompleteMatchFormatter class]]);

  AutocompleteMatchFormatter* firstSuggestion = wrappedGroups[0].suggestions[0];
  AutocompleteMatchFormatter* secondSuggestion =
      wrappedGroups[0].suggestions[1];

  // the `wrappedMatch` defaultSearchEngineIsGoogle should be true.
  EXPECT_TRUE(firstSuggestion.defaultSearchEngineIsGoogle);
  EXPECT_TRUE(secondSuggestion.defaultSearchEngineIsGoogle);

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

  wrappedGroups = [wrapper_ wrapAutocompleteResultInGroups:result];

  firstSuggestion = wrappedGroups[0].suggestions[0];
  secondSuggestion = wrappedGroups[0].suggestions[1];

  // the `wrappedMatch` defaultSearchEngineIsGoogle should now be false.
  EXPECT_FALSE(firstSuggestion.defaultSearchEngineIsGoogle);
  EXPECT_FALSE(secondSuggestion.defaultSearchEngineIsGoogle);

  // Change the default search provider back to Google.
  template_url_service->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));
}

/// Tests Wrapping a result that contains a pedal match.
TEST_F(AutocompleteResultWrapperTest, testWrapPedalMatch) {
  AutocompleteResult result;

  AutocompleteMatch match;

  scoped_refptr<OmniboxPedal> pedal =
      base::WrapRefCounted(new TestOmniboxPedalClearBrowsingData());
  match.actions.push_back(std::move(pedal));

  result.AppendMatches({match});

  NSArray<id<AutocompleteSuggestionGroup>>* wrappedGroups =
      [wrapper_ wrapAutocompleteResultInGroups:result];

  // The result should be wrapped into 2 groups where the first one is for
  // pedal.
  EXPECT_EQ(wrappedGroups.count, 2u);
  EXPECT_EQ(wrappedGroups[0].type, SuggestionGroupType::kPedalSuggestionGroup);
  EXPECT_EQ(wrappedGroups[1].type,
            SuggestionGroupType::kUnspecifiedSuggestionGroup);

  EXPECT_EQ(wrappedGroups[0].suggestions.count, 1u);
  EXPECT_EQ(wrappedGroups[1].suggestions.count, 1u);

  // Wrapped suggestions should be pedal.
  EXPECT_TRUE([wrappedGroups[0].suggestions[0]
      isKindOfClass:[PedalSuggestionWrapper class]]);
}
