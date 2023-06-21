// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class NoVarySearchHelperTester final {
 public:
  void AddUrl(const GURL& url, network::mojom::URLResponseHeadPtr head) {
    std::unique_ptr<PrefetchContainer> prefetch_container =
        std::make_unique<PrefetchContainer>(
            GlobalRenderFrameHostId(1234, 5678), url,
            PrefetchType(/*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kEager),
            blink::mojom::Referrer(),
            /*no_vary_search_expected=*/absl::nullopt,
            blink::mojom::SpeculationInjectionWorld::kNone,
            /*prefetch_document_manager=*/nullptr);

    MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                          std::move(head), "test body");
    prefetches_[url] = prefetch_container->GetWeakPtr();
    no_vary_search::SetNoVarySearchData(prefetch_container->GetWeakPtr());
    owned_prefetches_.push_back(std::move(prefetch_container));
  }

  base::WeakPtr<PrefetchContainer> MatchUrl(const GURL& url) {
    return no_vary_search::MatchUrl(url, prefetches_);
  }

  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
  GetAllForUrlWithoutRefAndQueryForTesting(const GURL& url) {
    return no_vary_search::GetAllForUrlWithoutRefAndQueryForTesting(
        url, prefetches_);
  }

 private:
  std::vector<std::unique_ptr<PrefetchContainer>> owned_prefetches_;
  std::map<GURL, base::WeakPtr<PrefetchContainer>> prefetches_;
};

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

class NoVarySearchHelperTest : public RenderViewHostTestHarness {
 public:
  NoVarySearchHelperTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { RenderViewHostTestHarness::SetUp(); }
};

TEST_F(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyVaryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>();
  const GURL test_url("https://a.com/index.html?a=2&b=3");
  helper->AddUrl(test_url, std::move(head));

  const auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_EQ(urls_with_no_vary_search.size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, test_url);
  EXPECT_THAT(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_params(),
              testing::UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->no_vary_params()
                  .empty());
  EXPECT_FALSE(urls_with_no_vary_search.at(0)
                   .second->GetNoVarySearchData()
                   ->vary_by_default());
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_on_key_order());
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/index.html?b=4&a=2&c=5")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/index.html?a=2")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html?b=4")));
}

TEST_F(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyNoVaryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({"a"});
  const GURL test_url = GURL("https://a.com/home.html?a=2&b=3");

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>();
  helper->AddUrl(test_url, std::move(head));
  const auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_EQ(urls_with_no_vary_search.size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, test_url);
  EXPECT_THAT(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->no_vary_params(),
              testing::UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_params()
                  .empty());
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_by_default());
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_on_key_order());
  EXPECT_TRUE(helper->MatchUrl(test_url));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/home.html?b=3")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://a.com/home.html?b=3&a=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html?b=4")));
}

TEST_F(NoVarySearchHelperTest, AddAndMatchUrlEmptyNoVaryParams) {
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

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>();
  helper->AddUrl(test_url, std::move(head));
  const auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_EQ(urls_with_no_vary_search.size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, test_url);
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->no_vary_params()
                  .empty());
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_params()
                  .empty());
  EXPECT_TRUE(urls_with_no_vary_search.at(0)
                  .second->GetNoVarySearchData()
                  ->vary_by_default());
  EXPECT_FALSE(urls_with_no_vary_search.at(0)
                   .second->GetNoVarySearchData()
                   ->vary_on_key_order());
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

TEST_F(NoVarySearchHelperTest, AddUrlWithoutNoVarySearchTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->parsed_headers = network::mojom::ParsedHeaders::New();

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>();
  GURL test_url("https://a.com/index.html?a=2&b=3");
  helper->AddUrl(test_url, std::move(head));

  auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_TRUE(urls_with_no_vary_search.empty());
  EXPECT_FALSE(helper->MatchUrl(test_url));
}

TEST_F(NoVarySearchHelperTest, DoNotPrefixMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>();

  // `no_match_url_num` and `no_match_url_foo` have the prefix
  // "https://example.com/index.html" but shouldn't match with
  // "https://example.com/index.html" via No-Vary-Search because the
  // non-ref/query parts don't match.
  //
  // The URLs are sorted (by `std::map`) in
  // `NoVarySearchHelperTester::prefixes_` in this order.
  const GURL matching_url_raw("https://example.com/index.html");
  const GURL matching_url_ref("https://example.com/index.html#ref");
  const GURL no_match_url_num("https://example.com/index.html111?a=3&b=3");
  const GURL matching_url_a_0("https://example.com/index.html?a=0&b=3");
  const GURL matching_url_a_1("https://example.com/index.html?a=1&b=3");
  const GURL matching_url_a_2("https://example.com/index.html?a=2&b=3");
  const GURL no_match_url_foo("https://example.com/index.htmlfoo?a=4&b=3");
  const GURL no_match_url_top("https://example.com/top.html?a=5&b=3");

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  // Call `AddUrl` in an order different from the URL sorted order to test that
  // `prefixes_` are sorted.
  helper->AddUrl(no_match_url_num, head->Clone());
  helper->AddUrl(matching_url_ref, head->Clone());
  helper->AddUrl(no_match_url_foo, head->Clone());
  helper->AddUrl(matching_url_a_1, head->Clone());
  helper->AddUrl(matching_url_a_0, network::mojom::URLResponseHead::New());
  helper->AddUrl(matching_url_a_2, head->Clone());
  helper->AddUrl(no_match_url_top, head->Clone());
  helper->AddUrl(matching_url_raw, head->Clone());

  // Even if the matching entries and non-matching entries (non-matching URLs
  // and `matching_url_a_0` without No-Vary-Search headers) are interleaved in
  // `NoVarySearchHelperTester::prefixes_`, all matching entries are retrieved
  // and can be matched.
  const auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(matching_url_raw);
  ASSERT_EQ(urls_with_no_vary_search.size(), 4u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, matching_url_raw);
  EXPECT_EQ(urls_with_no_vary_search.at(1).first, matching_url_ref);
  EXPECT_EQ(urls_with_no_vary_search.at(2).first, matching_url_a_1);
  EXPECT_EQ(urls_with_no_vary_search.at(3).first, matching_url_a_2);

  EXPECT_TRUE(
      helper->MatchUrl(GURL("https://example.com/index.html?b=4&a=2&c=5")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://example.com/index.html?a=1")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://example.com/index.html?a=2")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://example.com/index.html111?a=3")));
  EXPECT_TRUE(helper->MatchUrl(GURL("https://example.com/index.htmlfoo?a=4")));

  // `matching_url_a_0` shouldn't match due to lack of the No-Vary-Search
  // header.
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=0")));

  // Shouldn't match due to different non-ref/query parts of URLs.
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=3")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=5")));
}

}  // namespace
}  // namespace content
