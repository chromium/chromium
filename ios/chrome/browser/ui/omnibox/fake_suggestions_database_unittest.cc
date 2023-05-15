// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/fake_suggestions_database.h"

#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeSuggestionsDatabaseTest : public testing::Test {
 public:
  FakeSuggestionsDatabaseTest() : template_url_service_(nullptr, 0) {
    fake_suggestions_database_ =
        std::make_unique<FakeSuggestionsDatabase>(&template_url_service_);
  }

  GURL GetSearchURL(const std::u16string& search_terms);
  FakeSuggestionsDatabase& fake_suggestions_database() {
    return *fake_suggestions_database_;
  }
  const TemplateURL* default_search_provider() {
    return template_url_service_.GetDefaultSearchProvider();
  }

 protected:
  void SetUp() override {
    TemplateURL* default_provider = template_url_service_.Add(
        std::make_unique<TemplateURL>(default_search_provider()->data()));
    template_url_service_.SetUserSelectedDefaultSearchProvider(
        default_provider);
  }

 private:
  std::unique_ptr<FakeSuggestionsDatabase> fake_suggestions_database_;
  TemplateURLService template_url_service_;
};

GURL FakeSuggestionsDatabaseTest::GetSearchURL(
    const std::u16string& search_terms) {
  TemplateURLRef::SearchTermsArgs search_terms_args(search_terms);
  const auto& search_terms_data = template_url_service_.search_terms_data();
  std::string search_url =
      default_search_provider()->suggestions_url_ref().ReplaceSearchTerms(
          search_terms_args, search_terms_data);
  return GURL(search_url);
}

TEST_F(FakeSuggestionsDatabaseTest, HasFakeSuggestions) {
  GURL url_1 = GetSearchURL(u"");
  std::string fake_suggestions_1 = "FakeSuggestions1";

  GURL url_2 = GetSearchURL(u"example query 2");
  std::string fake_suggestions_2 = "FakeSuggestions2";

  GURL url_no_fake = GetSearchURL(u"no fake suggestions");

  FakeSuggestionsDatabase& database = fake_suggestions_database();

  database.SetFakeSuggestions(url_1, fake_suggestions_1);
  database.SetFakeSuggestions(url_2, fake_suggestions_2);

  EXPECT_TRUE(database.HasFakeSuggestions(url_1));
  EXPECT_TRUE(database.HasFakeSuggestions(url_2));
  EXPECT_FALSE(database.HasFakeSuggestions(url_no_fake));
}

TEST_F(FakeSuggestionsDatabaseTest, GetSetFakeSuggestions) {
  GURL url_1 = GetSearchURL(u"");
  std::string fake_suggestions_1 = "FakeSuggestions1";

  GURL url_2 = GetSearchURL(u"oneterm");
  std::string fake_suggestions_2 = "FakeSuggestions2";

  GURL url_3 = GetSearchURL(u"multiple terms");
  std::string fake_suggestions_3 = "FakeSuggestions3";

  FakeSuggestionsDatabase& database = fake_suggestions_database();

  database.SetFakeSuggestions(url_1, fake_suggestions_1);
  database.SetFakeSuggestions(url_2, fake_suggestions_2);
  database.SetFakeSuggestions(url_3, fake_suggestions_3);

  EXPECT_EQ(fake_suggestions_1, database.GetFakeSuggestions(url_1));
  EXPECT_EQ(fake_suggestions_2, database.GetFakeSuggestions(url_2));
  EXPECT_EQ(fake_suggestions_3, database.GetFakeSuggestions(url_3));
}
