// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_renderer_host.h"
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

class NoVarySearchHelperTester final {
 public:
  explicit NoVarySearchHelperTester(bool use_prefetches_by_key)
      : use_prefetches_by_key_(use_prefetches_by_key),
        prev_document_token_(base::UnguessableToken::CreateForTesting(
            document_token_->GetHighForSerialization(),
            document_token_->GetLowForSerialization() - 1)),
        next_document_token_(base::UnguessableToken::CreateForTesting(
            document_token_->GetHighForSerialization(),
            document_token_->GetLowForSerialization() + 1)) {}

  PrefetchContainer* AddUrl(RenderFrameHostImpl& referring_render_frame_host,
                            const GURL& url,
                            network::mojom::URLResponseHeadPtr head) {
    auto prefetch_container = CreatePrefetchContainer(
        referring_render_frame_host, document_token_, url, std::move(head));
    prefetches_[url] = prefetch_container;
    prefetches_by_key_[prefetch_container->key()] = prefetch_container;

    // Also add `PrefetchContainer` with different `DocumentToken`s, to test
    // that `PrefetchContainer` with different `DocumentToken`s are not
    // iterated.
    // Ignore all query parameters to make it easier to be matched by mistake.
    network::mojom::URLResponseHeadPtr head_for_different_document =
        CreateHead();
    head_for_different_document->parsed_headers->no_vary_search_with_parse_error
        ->get_no_vary_search()
        ->search_variance =
        network::mojom::SearchParamsVariance::NewVaryParams({});
    auto next_prefetch_container = CreatePrefetchContainer(
        referring_render_frame_host, next_document_token_, url,
        head_for_different_document.Clone());
    prefetches_by_key_[next_prefetch_container->key()] =
        next_prefetch_container;
    auto prev_prefetch_container = CreatePrefetchContainer(
        referring_render_frame_host, prev_document_token_, url,
        std::move(head_for_different_document));
    prefetches_by_key_[prev_prefetch_container->key()] =
        prev_prefetch_container;

    return prefetch_container.get();
  }

  PrefetchContainer* MatchUrl(const GURL& url) {
    if (use_prefetches_by_key_) {
      return no_vary_search::MatchUrl(
                 PrefetchContainer::Key(document_token_, url),
                 prefetches_by_key_)
          .get();
    } else {
      return no_vary_search::MatchUrl(url, prefetches_).get();
    }
  }

  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
  GetAllForUrlWithoutRefAndQueryForTesting(const GURL& url) {
    if (use_prefetches_by_key_) {
      return no_vary_search::GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(document_token_, url), prefetches_by_key_);
    } else {
      return no_vary_search::GetAllForUrlWithoutRefAndQueryForTesting(
          url, prefetches_);
    }
  }

 private:
  base::WeakPtr<PrefetchContainer> CreatePrefetchContainer(
      RenderFrameHostImpl& referring_render_frame_host,
      const blink::DocumentToken& document_token,
      const GURL& url,
      network::mojom::URLResponseHeadPtr head) {
    std::unique_ptr<PrefetchContainer> prefetch_container =
        std::make_unique<PrefetchContainer>(
            referring_render_frame_host, document_token, url,
            PrefetchType(PreloadingTriggerType::kSpeculationRule,
                         /*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kEager),
            blink::mojom::Referrer(),
            /*no_vary_search_expected=*/std::nullopt,
            /*prefetch_document_manager=*/nullptr);

    MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                          std::move(head), "test body");
    auto weak_prefetch_container = prefetch_container->GetWeakPtr();
    owned_prefetches_.push_back(std::move(prefetch_container));

    return weak_prefetch_container;
  }

  std::vector<std::unique_ptr<PrefetchContainer>> owned_prefetches_;
  std::map<GURL, base::WeakPtr<PrefetchContainer>> prefetches_;

  const bool use_prefetches_by_key_;

  const blink::DocumentToken document_token_{};
  // Different DocumentTokens are prepared so that `prev_document_token_` <
  // `document_token_` < `next_document_token_`.
  const blink::DocumentToken prev_document_token_;
  const blink::DocumentToken next_document_token_;
  std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>
      prefetches_by_key_;
};

// bool `GetParam()` indicates whether `MatchUrl` should operate on
// `prefetches_by_key_`.
class NoVarySearchHelperTest : public RenderViewHostTestHarness,
                               public ::testing::WithParamInterface<bool> {
 public:
  NoVarySearchHelperTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { RenderViewHostTestHarness::SetUp(); }

  RenderFrameHostImpl* main_rfhi() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }
};

TEST_P(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyVaryParams) {
  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());
  const GURL test_url("https://a.com/index.html?a=2&b=3");
  auto* prefetch_container =
      helper->AddUrl(*main_rfhi(), test_url, std::move(head));

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
  EXPECT_EQ(helper->MatchUrl(GURL("https://a.com/index.html?b=4&a=2&c=5")),
            prefetch_container);
  EXPECT_EQ(helper->MatchUrl(GURL("https://a.com/index.html?a=2")),
            prefetch_container);
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/index.html?b=4")));
}

TEST_P(NoVarySearchHelperTest, AddAndMatchUrlNonEmptyNoVaryParams) {
  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({"a"});
  const GURL test_url = GURL("https://a.com/home.html?a=2&b=3");

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());
  auto* prefetch_container =
      helper->AddUrl(*main_rfhi(), test_url, std::move(head));
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
  EXPECT_EQ(helper->MatchUrl(test_url), prefetch_container);
  EXPECT_EQ(helper->MatchUrl(GURL("https://a.com/home.html?b=3")),
            prefetch_container);
  EXPECT_EQ(helper->MatchUrl(GURL("https://a.com/home.html?b=3&a=4")),
            prefetch_container);
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://a.com/home.html?b=4")));
}

TEST_P(NoVarySearchHelperTest, AddAndMatchUrlEmptyNoVaryParams) {
  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({});
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->vary_on_key_order = false;
  const GURL test_url = GURL("https://a.com/away.html?a=2&b=3&c=6");

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());
  auto* prefetch_container =
      helper->AddUrl(*main_rfhi(), test_url, std::move(head));
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
  EXPECT_EQ(helper->MatchUrl(GURL("https://a.com/away.html?b=3&c=6&a=2")),
            prefetch_container);
  EXPECT_EQ(helper->MatchUrl(test_url), prefetch_container);
}

TEST_P(NoVarySearchHelperTest, AddUrlWithoutNoVarySearchTest) {
  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->parsed_headers = network::mojom::ParsedHeaders::New();

  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());
  GURL test_url("https://a.com/index.html?a=2&b=3");
  auto* prefetch_container =
      helper->AddUrl(*main_rfhi(), test_url, std::move(head));

  auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url);
  ASSERT_EQ(urls_with_no_vary_search.size(), 1u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, test_url);
  EXPECT_EQ(helper->MatchUrl(test_url), prefetch_container);
}

TEST_P(NoVarySearchHelperTest, DoNotPrefixMatch) {
  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());

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
  auto* pc_no_match_url_num =
      helper->AddUrl(*main_rfhi(), no_match_url_num, head->Clone());
  auto* pc_matching_url_ref =
      helper->AddUrl(*main_rfhi(), matching_url_ref, head->Clone());
  auto* pc_no_match_url_foo =
      helper->AddUrl(*main_rfhi(), no_match_url_foo, head->Clone());
  auto* pc_matching_url_a_1 =
      helper->AddUrl(*main_rfhi(), matching_url_a_1, head->Clone());
  auto* pc_matching_url_a_0 = helper->AddUrl(
      *main_rfhi(), matching_url_a_0, network::mojom::URLResponseHead::New());
  auto* pc_matching_url_a_2 =
      helper->AddUrl(*main_rfhi(), matching_url_a_2, head->Clone());
  auto* pc_no_match_url_top =
      helper->AddUrl(*main_rfhi(), no_match_url_top, head->Clone());
  auto* pc_matching_url_raw =
      helper->AddUrl(*main_rfhi(), matching_url_raw, head->Clone());

  // Even if the matching entries and non-matching entries (non-matching URLs
  // and `matching_url_a_0` without No-Vary-Search headers) are interleaved in
  // `NoVarySearchHelperTester::prefixes_`, all matching entries are retrieved
  // and can be matched.
  const auto urls_with_no_vary_search =
      helper->GetAllForUrlWithoutRefAndQueryForTesting(matching_url_raw);
  ASSERT_EQ(urls_with_no_vary_search.size(), 5u);
  EXPECT_EQ(urls_with_no_vary_search.at(0).first, matching_url_raw);
  EXPECT_EQ(urls_with_no_vary_search.at(1).first, matching_url_ref);
  EXPECT_EQ(urls_with_no_vary_search.at(2).first, matching_url_a_0);
  EXPECT_EQ(urls_with_no_vary_search.at(3).first, matching_url_a_1);
  EXPECT_EQ(urls_with_no_vary_search.at(4).first, matching_url_a_2);

  EXPECT_EQ(
      helper->MatchUrl(GURL("https://example.com/index.html?b=4&a=2&c=5")),
      pc_matching_url_a_2);
  EXPECT_EQ(helper->MatchUrl(matching_url_a_0), pc_matching_url_a_0);
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=0")));
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.html?a=1")),
            pc_matching_url_a_1);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.html?a=2")),
            pc_matching_url_a_2);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.html111?a=3")),
            pc_no_match_url_num);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.htmlfoo?a=4")),
            pc_no_match_url_foo);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/top.html?a=5")),
            pc_no_match_url_top);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.html?b=3")),
            pc_matching_url_raw);
  EXPECT_EQ(helper->MatchUrl(matching_url_ref), pc_matching_url_ref);
  EXPECT_EQ(helper->MatchUrl(GURL("https://example.com/index.html?b=3#ref")),
            pc_matching_url_raw);

  // `matching_url_a_0` shouldn't match due to lack of the No-Vary-Search
  // header.
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=0")));

  // Shouldn't match due to different non-ref/query parts of URLs.
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=3")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=4")));
  EXPECT_FALSE(helper->MatchUrl(GURL("https://example.com/index.html?a=5")));
}

TEST_P(NoVarySearchHelperTest, DoNotMatchDifferentDocumentToken) {
  std::unique_ptr<NoVarySearchHelperTester> helper =
      std::make_unique<NoVarySearchHelperTester>(GetParam());

  network::mojom::URLResponseHeadPtr head = CreateHead();
  head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
      ->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  const GURL url("https://example.com/index.html?a=2&b=3");
  const GURL test_url("https://example.com/index.html?a=2");
  const GURL foo_url("https://example.com/index.html?foo");

  auto* prefetch_container = helper->AddUrl(*main_rfhi(), url, std::move(head));

  // Here, `NoVarySearchHelperTester::prefetches_by_key_` have three keys,
  // sorted in the order:
  // - (prev_document_token_, url)
  // - (document_token_, url)
  // - (next_document_token_, url)
  // Even though the consecutive entries have the same URL, we shouldn't include
  // the 1st/3rd ones in the matching results because DocumentToken is
  // different.

  EXPECT_EQ(helper->MatchUrl(url), prefetch_container);
  EXPECT_EQ(helper->GetAllForUrlWithoutRefAndQueryForTesting(url).size(), 1u);

  EXPECT_EQ(helper->MatchUrl(test_url), prefetch_container);
  EXPECT_EQ(helper->GetAllForUrlWithoutRefAndQueryForTesting(test_url).size(),
            1u);

  EXPECT_FALSE(helper->MatchUrl(foo_url));
  EXPECT_EQ(helper->GetAllForUrlWithoutRefAndQueryForTesting(foo_url).size(),
            1u);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NoVarySearchHelperTest,
    testing::Bool());

}  // namespace
}  // namespace content
