// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

network::mojom::URLResponseHeadPtr CreateHead() {
  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->parsed_headers = network::mojom::ParsedHeaders::New();
  head->parsed_headers->no_vary_search_with_parse_error =
      network::mojom::NoVarySearchWithParseError::NewNoVarySearch(
          network::mojom::NoVarySearch::New());
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->vary_on_key_order = true;
  return head;
}

TEST(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyVaryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  scoped_refptr<NoVarySearchHelper> helper =
      base::MakeRefCounted<NoVarySearchHelper>();
  const GURL test_url("https://a.com/index.html?a=2&b=3");
  helper->AddUrl(test_url, *head);

  const auto* urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_TRUE(urls_with_no_vary_search);
  ASSERT_EQ(urls_with_no_vary_search->size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search->at(0).first, test_url);
  EXPECT_THAT(urls_with_no_vary_search->at(0).second.vary_params(),
              testing::UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.no_vary_params().empty());
  EXPECT_FALSE(urls_with_no_vary_search->at(0).second.vary_by_default());
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_on_key_order());
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/index.html?b=4&a=2&c=5")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/index.html?a=2")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html?b=4")));
}

TEST(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyNoVaryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({"a"});
  const GURL test_url = GURL("https://a.com/home.html?a=2&b=3");

  scoped_refptr<NoVarySearchHelper> helper =
      base::MakeRefCounted<NoVarySearchHelper>();
  helper->AddUrl(test_url, *head);
  const auto* urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_TRUE(urls_with_no_vary_search);
  ASSERT_EQ(urls_with_no_vary_search->size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search->at(0).first, test_url);
  EXPECT_THAT(urls_with_no_vary_search->at(0).second.no_vary_params(),
              testing::UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_params().empty());
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_by_default());
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_on_key_order());
  EXPECT_TRUE(helper->MatchUrl(test_url));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/home.html?b=3")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/home.html?b=3&a=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html?b=4")));
}

TEST(NoVarySearchHelperTest, AddAndMatchUrlEmptyNoVaryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({});
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->vary_on_key_order = false;
  const GURL test_url = GURL("https://a.com/away.html?a=2&b=3&c=6");

  scoped_refptr<NoVarySearchHelper> helper =
      base::MakeRefCounted<NoVarySearchHelper>();
  helper->AddUrl(test_url, *head);
  const auto* urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_TRUE(urls_with_no_vary_search);
  ASSERT_EQ(urls_with_no_vary_search->size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search->at(0).first, test_url);
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.no_vary_params().empty());
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_params().empty());
  EXPECT_TRUE(urls_with_no_vary_search->at(0).second.vary_by_default());
  EXPECT_FALSE(urls_with_no_vary_search->at(0).second.vary_on_key_order());
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/away.html?b=3")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/away.html?b=3&a=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/away.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/away.html?b=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/away.html?b=3&c=6&a=4")));
  EXPECT_FALSE(
      helper->MatchUrl(GURL("https://a.com/away.html?a=2&b=3&c=6&d=5")));
  EXPECT_FALSE(
      helper->MatchUrl(GURL("https://a.com/away.html?a=2&b=3&c=6&a=5")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/away.html?b=3&c=6&a=2")));
  EXPECT_TRUE(helper->MatchUrl(test_url));
}

TEST(NoVarySearchHelperTests, AddUrlWithoutNoVarySearchTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHead head;
  head.parsed_headers = network::mojom::ParsedHeaders::New();

  scoped_refptr<NoVarySearchHelper> helper =
      base::MakeRefCounted<NoVarySearchHelper>();
  GURL test_url("https://a.com/index.html?a=2&b=3");
  helper->AddUrl(test_url, head);

  auto* urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_FALSE(urls_with_no_vary_search);
  EXPECT_FALSE(helper->MatchUrl(test_url));
}

}  // namespace
}  // namespace content
