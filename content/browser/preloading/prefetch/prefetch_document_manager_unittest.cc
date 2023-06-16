// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void PrefetchUrl(
      base::WeakPtr<PrefetchContainer> prefetch_container) override {
    prefetch_container->DisablePrecogLoggingForTest();
    prefetches_.push_back(prefetch_container);
  }

  void PrepareToServe(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container) override {
    prefetches_prepared_to_serve_.emplace_back(url, prefetch_container);
  }

  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_;
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
      prefetches_prepared_to_serve_;
};

class PrefetchDocumentManagerTest : public RenderViewHostTestHarness {
 public:
  PrefetchDocumentManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrefetchUseContentRefactor,
        {{"proxy_host", "https://testproxyhost.com"}});
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->NavigateAndCommit(GetSameOriginUrl("/"));

    prefetch_service_ =
        std::make_unique<TestPrefetchService>(browser_context_.get());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& GetPrimaryMainFrame() {
    return web_contents_->GetPrimaryPage().GetMainDocument();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetSameSiteCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.com" + path);
  }

  void NavigateMainframeRendererTo(const GURL& url) {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(url,
                                                     &GetPrimaryMainFrame());
    simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
    simulator->Start();
  }

  const std::vector<base::WeakPtr<PrefetchContainer>>& GetPrefetches() {
    return prefetch_service_->prefetches_;
  }

  const std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>&
  GetPrefetchesPreparedToServe() {
    return prefetch_service_->prefetches_prepared_to_serve_;
  }

  // Used to make sure that No-Vary-Search parsing error/warning message is sent
  // to DevTools console.
  std::string TriggerNoVarySearchParseErrorAndGetConsoleMessage(
      network::mojom::NoVarySearchParseError parse_error) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        network::features::kPrefetchNoVarySearch);
    // Used to create responses.
    const net::IsolationInfo info;
    // Process the candidates with the |PrefetchDocumentManager| for the current
    // document.
    auto* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            &GetPrimaryMainFrame());
    prefetch_document_manager->EnableNoVarySearchSupport();

    // Create list of SpeculationCandidatePtrs.
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    // Create candidate for private cross-origin prefetch. This candidate should
    // be prefetched by |PrefetchDocumentManager|.
    auto candidate1 = blink::mojom::SpeculationCandidate::New();
    const auto test_url = GetCrossOriginUrl("/candidate1.html?a=2&b=3");
    candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate1->requires_anonymous_client_ip_when_cross_origin = false;
    candidate1->url = test_url;
    candidate1->referrer = blink::mojom::Referrer::New();

    candidates.push_back(std::move(candidate1));

    prefetch_document_manager->ProcessCandidates(candidates,
                                                 /*devtools_observer=*/nullptr);
    // Now call TakePrefetchedResponse
    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    head->parsed_headers = network::mojom::ParsedHeaders::New();
    head->parsed_headers->no_vary_search_with_parse_error =
        network::mojom::NoVarySearchWithParseError::NewParseError(parse_error);

    GetPrefetches()[0]->TakeStreamingURLLoader(
        MakeServableStreamingURLLoaderForTest(std::move(head), "empty"));
    GetPrefetches()[0]->OnPrefetchedResponseHeadReceived();

    auto& test_rfh = static_cast<TestRenderFrameHost&>(GetPrimaryMainFrame());
    return test_rfh.GetConsoleMessages()[0];
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<TestPrefetchService> prefetch_service_;
};

TEST_F(PrefetchDocumentManagerTest, PopulateNoVarySearchHint) {
  // Process the candidates with the |PrefetchDocumentManager| for the current
  // document.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &GetPrimaryMainFrame());
  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  // Create candidate for private cross-origin prefetch. This candidate should
  // be prefetched by |PrefetchDocumentManager|.
  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  const auto test_url1 = GetCrossOriginUrl("/candidate1.html?a=2&b=3");
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = false;
  candidate1->url = test_url1;
  candidate1->referrer = blink::mojom::Referrer::New();
  candidate1->no_vary_search_hint = network::mojom::NoVarySearch::New();
  candidate1->no_vary_search_hint->vary_on_key_order = false;
  candidate1->no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams({"a"});

  auto candidate2 = blink::mojom::SpeculationCandidate::New();
  const auto test_url2 = GetCrossOriginUrl("/candidate2.html?a=2&b=3");
  candidate2->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate2->requires_anonymous_client_ip_when_cross_origin = false;
  candidate2->url = test_url2;
  candidate2->referrer = blink::mojom::Referrer::New();
  candidate2->no_vary_search_hint = network::mojom::NoVarySearch::New();
  candidate2->no_vary_search_hint->vary_on_key_order = true;
  candidate2->no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewVaryParams({"a"});

  auto candidate3 = blink::mojom::SpeculationCandidate::New();
  const auto test_url3 = GetCrossOriginUrl("/candidate3.html?a=2&b=3");
  candidate3->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate3->requires_anonymous_client_ip_when_cross_origin = false;
  candidate3->url = test_url3;
  candidate3->referrer = blink::mojom::Referrer::New();

  candidates.push_back(std::move(candidate1));
  candidates.push_back(std::move(candidate2));
  candidates.push_back(std::move(candidate3));

  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);

  ASSERT_EQ(GetPrefetches().size(), 3u);
  {
    auto& prefetch = GetPrefetches()[0];
    ASSERT_TRUE(prefetch);
    ASSERT_TRUE(prefetch->GetNoVarySearchHint().has_value());
    EXPECT_FALSE(prefetch->GetNoVarySearchHint()->vary_on_key_order());
    EXPECT_THAT(prefetch->GetNoVarySearchHint()->no_vary_params(),
                UnorderedElementsAreArray({"a"}));
  }
  {
    auto& prefetch = GetPrefetches()[1];
    ASSERT_TRUE(prefetch);
    ASSERT_TRUE(prefetch->GetNoVarySearchHint().has_value());
    EXPECT_TRUE(prefetch->GetNoVarySearchHint()->vary_on_key_order());
    EXPECT_THAT(prefetch->GetNoVarySearchHint()->vary_params(),
                UnorderedElementsAreArray({"a"}));
  }
  {
    auto& prefetch = GetPrefetches()[2];
    ASSERT_TRUE(prefetch);
    EXPECT_FALSE(prefetch->GetNoVarySearchHint().has_value());
  }
}

TEST_F(PrefetchDocumentManagerTest, ProcessNoVarySearchResponse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kPrefetchNoVarySearch);
  // Used to create responses.
  const net::IsolationInfo info;
  // Process the candidates with the |PrefetchDocumentManager| for the current
  // document.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &GetPrimaryMainFrame());
  prefetch_document_manager->EnableNoVarySearchSupport();
  {
    // Create list of SpeculationCandidatePtrs.
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    // Create candidate for private cross-origin prefetch. This candidate should
    // be prefetched by |PrefetchDocumentManager|.
    auto candidate1 = blink::mojom::SpeculationCandidate::New();
    const auto test_url = GetCrossOriginUrl("/candidate1.html?a=2&b=3");
    candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate1->requires_anonymous_client_ip_when_cross_origin = false;
    candidate1->url = test_url;
    candidate1->referrer = blink::mojom::Referrer::New();

    candidates.push_back(std::move(candidate1));

    prefetch_document_manager->ProcessCandidates(candidates,
                                                 /*devtools_observer=*/nullptr);

    // Now call TakePrefetchedResponse
    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    head->parsed_headers = network::mojom::ParsedHeaders::New();
    head->parsed_headers->no_vary_search_with_parse_error =
        network::mojom::NoVarySearchWithParseError::NewNoVarySearch(
            network::mojom::NoVarySearch::New());
    head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
        ->vary_on_key_order = true;
    head->parsed_headers->no_vary_search_with_parse_error->get_no_vary_search()
        ->search_variance =
        network::mojom::SearchParamsVariance::NewVaryParams({"a"});

    GetPrefetches()[0]->TakeStreamingURLLoader(
        MakeServableStreamingURLLoaderForTest(std::move(head), "empty"));
    GetPrefetches()[0]->OnPrefetchedResponseHeadReceived();

    const auto urls_with_no_vary_search =
        prefetch_document_manager->GetAllForUrlWithoutRefAndQueryForTesting(
            test_url);
    ASSERT_EQ(urls_with_no_vary_search.size(), 1u);
    EXPECT_EQ(urls_with_no_vary_search.at(0).first, test_url);
    const absl::optional<net::HttpNoVarySearchData>& nvs =
        urls_with_no_vary_search.at(0).second->GetNoVarySearchData();
    ASSERT_TRUE(nvs);
    EXPECT_THAT(nvs->vary_params(), UnorderedElementsAreArray({"a"}));
    EXPECT_THAT(nvs->no_vary_params(), IsEmpty());
    EXPECT_FALSE(nvs->vary_by_default());
    EXPECT_TRUE(nvs->vary_on_key_order());
    EXPECT_TRUE(prefetch_document_manager->MatchUrl(
        GetCrossOriginUrl("/candidate1.html?b=4&a=2&c=5")));
    EXPECT_TRUE(prefetch_document_manager->MatchUrl(
        GetCrossOriginUrl("/candidate1.html?a=2")));
    EXPECT_FALSE(prefetch_document_manager->MatchUrl(
        GetCrossOriginUrl("/candidate1.html")));
    EXPECT_FALSE(prefetch_document_manager->MatchUrl(
        GetCrossOriginUrl("/candidate1.html?b=4")));
  }
  {
    const auto test_url = GetCrossOriginUrl("/candidate2.html?a=2&b=3");
    auto candidate1 = blink::mojom::SpeculationCandidate::New();
    candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate1->requires_anonymous_client_ip_when_cross_origin = false;
    candidate1->url = test_url;
    candidate1->referrer = blink::mojom::Referrer::New();

    // Create list of SpeculationCandidatePtrs.
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    candidates.emplace_back(std::move(candidate1));
    prefetch_document_manager->ProcessCandidates(candidates,
                                                 /*devtools_observer=*/nullptr);

    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    head->parsed_headers = network::mojom::ParsedHeaders::New();

    GetPrefetches().back()->TakeStreamingURLLoader(
        MakeServableStreamingURLLoaderForTest(std::move(head), "empty"));
    GetPrefetches().back()->OnPrefetchedResponseHeadReceived();

    const auto urls_with_no_vary_search =
        prefetch_document_manager->GetAllForUrlWithoutRefAndQueryForTesting(
            test_url);
    ASSERT_TRUE(urls_with_no_vary_search.empty());
  }

  NavigateMainframeRendererTo(GetCrossOriginUrl("/candidate2.html?a=2&b=3"));
  EXPECT_EQ(GetPrefetchesPreparedToServe()[0].first,
            GetCrossOriginUrl("/candidate2.html?a=2&b=3"));
  EXPECT_EQ(GetCrossOriginUrl("/candidate2.html?a=2&b=3"),
            GetPrefetchesPreparedToServe()[0].second->GetURL());

  NavigateMainframeRendererTo(
      GetCrossOriginUrl("/candidate1.html?b=4&a=2&c=5"));
  EXPECT_EQ(GetPrefetchesPreparedToServe()[1].first,
            GetCrossOriginUrl("/candidate1.html?b=4&a=2&c=5"));
  EXPECT_EQ(GetPrefetchesPreparedToServe()[1].second->GetURL(),
            GetCrossOriginUrl("/candidate1.html?a=2&b=3"));

  NavigateMainframeRendererTo(
      GetCrossOriginUrl("/not_prefetched.html?b=4&a=2&c=5"));
  EXPECT_EQ(GetPrefetchesPreparedToServe().size(), 2u);

  // Cover the case where we want to navigate again to the same prefetched
  // Url.
  // Simulate that we've already navigated to prefetched URL.
  GetPrefetchesPreparedToServe()[0].second->OnReturnPrefetchToServe(
      /*served=*/true);
  // Try to navigate again to the same URL.
  NavigateMainframeRendererTo(GetCrossOriginUrl("/candidate2.html?a=2&b=3"));
  EXPECT_EQ(GetPrefetchesPreparedToServe().size(), 3u);
  // PrepareToServe("/candidate2.html?a=2&b=3") is anyway called, but in
  // non-test environment this will be merged or ignored later in
  // PrefetchService.
  EXPECT_EQ(GetPrefetchesPreparedToServe()[2].first,
            GetCrossOriginUrl("/candidate2.html?a=2&b=3"));
  EXPECT_EQ(GetPrefetchesPreparedToServe()[2].second->GetURL(),
            GetCrossOriginUrl("/candidate2.html?a=2&b=3"));

  // Cover the case where we want to navigate to a URL with No-Vary-Search for
  // which the PrefetchContainer WeakPtr is not valid anymore.
  prefetch_document_manager->ReleasePrefetchContainer(
      GetPrefetchesPreparedToServe()[1].second->GetURL());
  DCHECK(!GetPrefetchesPreparedToServe()[1].second);
  NavigateMainframeRendererTo(
      GetCrossOriginUrl("/candidate1.html?b=4&a=2&c=5"));
  EXPECT_EQ(GetPrefetchesPreparedToServe().size(), 3u);
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithDefaultValue) {
  EXPECT_THAT(TriggerNoVarySearchParseErrorAndGetConsoleMessage(
                  network::mojom::NoVarySearchParseError::kDefaultValue),
              testing::HasSubstr("is equivalent to the default behavior"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithNotDictionary) {
  EXPECT_THAT(TriggerNoVarySearchParseErrorAndGetConsoleMessage(
                  network::mojom::NoVarySearchParseError::kNotDictionary),
              testing::HasSubstr("is not a dictionary"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithUnknownDictionaryKey) {
  EXPECT_THAT(
      TriggerNoVarySearchParseErrorAndGetConsoleMessage(
          network::mojom::NoVarySearchParseError::kUnknownDictionaryKey),
      testing::HasSubstr("contains unknown dictionary keys"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithNonBooleanKeyOrder) {
  EXPECT_THAT(
      TriggerNoVarySearchParseErrorAndGetConsoleMessage(
          network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder),
      testing::HasSubstr(
          "contains a \"key-order\" dictionary value that is not a boolean"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithParamsNotStringList) {
  EXPECT_THAT(TriggerNoVarySearchParseErrorAndGetConsoleMessage(
                  network::mojom::NoVarySearchParseError::kParamsNotStringList),
              testing::HasSubstr(
                  "contains a \"params\" dictionary value that is not a list"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithExceptNotStringList) {
  EXPECT_THAT(
      TriggerNoVarySearchParseErrorAndGetConsoleMessage(
          network::mojom::NoVarySearchParseError::kExceptNotStringList),
      testing::HasSubstr(
          "contains an \"except\" dictionary value that is not a list"));
}

TEST_F(PrefetchDocumentManagerTest,
       ProcessNoVarySearchResponseWithExceptWithoutTrueParams) {
  EXPECT_THAT(
      TriggerNoVarySearchParseErrorAndGetConsoleMessage(
          network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams),
      testing::HasSubstr(
          "contains an \"except\" dictionary key, without the \"params\""));
}

TEST_F(PrefetchDocumentManagerTest, ProcessSpeculationCandidates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      network::features::kPrefetchNoVarySearch);
  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto referrer = blink::mojom::Referrer::New();
  referrer->url = GetSameOriginUrl("/referrer");

  // Create candidate for private cross-origin prefetch. This candidate should
  // be prefetched by |PrefetchDocumentManager|.
  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = referrer->Clone();
  candidate1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate1));

  // Create candidate for non-private cross-origin prefetch. This candidate
  // should be prefetched by |PrefetchDocumentManager|.
  auto candidate2 = blink::mojom::SpeculationCandidate::New();
  candidate2->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate2->requires_anonymous_client_ip_when_cross_origin = false;
  candidate2->url = GetCrossOriginUrl("/candidate2.html");
  candidate2->referrer = referrer->Clone();
  candidate2->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate2));

  // Create candidate for non-private cross-origin prefetch. This candidate
  // should be prefetched by |PrefetchDocumentManager|.
  auto candidate3 = blink::mojom::SpeculationCandidate::New();
  candidate3->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate3->requires_anonymous_client_ip_when_cross_origin = false;
  candidate3->url = GetSameOriginUrl("/candidate3.html");
  candidate3->referrer = referrer->Clone();
  candidate3->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate3));

  // Create candidate for private cross-origin prefetch with subresources. This
  // candidate should not be prefetched by |PrefetchDocumentManager|.
  auto candidate4 = blink::mojom::SpeculationCandidate::New();
  candidate4->action =
      blink::mojom::SpeculationAction::kPrefetchWithSubresources;
  candidate4->requires_anonymous_client_ip_when_cross_origin = true;
  candidate4->url = GetCrossOriginUrl("/candidate4.html");
  candidate4->referrer = referrer->Clone();
  candidate4->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate4));

  // Create candidate for prerender. This candidate should not be prefetched by
  // |PrefetchDocumentManager|.
  auto candidate5 = blink::mojom::SpeculationCandidate::New();
  candidate5->action = blink::mojom::SpeculationAction::kPrerender;
  candidate5->requires_anonymous_client_ip_when_cross_origin = false;
  candidate5->url = GetCrossOriginUrl("/candidate5.html");
  candidate5->referrer = referrer->Clone();
  candidate5->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate5));

  // Create candidate for private cross-origin prefetch with default eagerness.
  // This candidate should be prefetched by |PrefetchDocumentManager|.
  auto candidate6 = blink::mojom::SpeculationCandidate::New();
  candidate6->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate6->requires_anonymous_client_ip_when_cross_origin = true;
  candidate6->url = GetCrossOriginUrl("/candidate6.html");
  candidate6->referrer = referrer->Clone();
  candidate6->eagerness = blink::mojom::SpeculationEagerness::kConservative;
  candidates.push_back(std::move(candidate6));

  // Create candidate for same-site prefetch. This candidate should
  // be prefetched by |PrefetchDocumentManager|.
  auto candidate7 = blink::mojom::SpeculationCandidate::New();
  candidate7->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate7->requires_anonymous_client_ip_when_cross_origin = false;
  candidate7->url = GetSameSiteCrossOriginUrl("/candidate7.html");
  candidate7->referrer = referrer->Clone();
  candidate7->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate7));

  // Create candidate for same-origin prefetch that requires a proxy if
  // redirected to a cross-origin URL. This candidate should be prefetched by
  // |PrefetchDocumentManager|.
  auto candidate8 = blink::mojom::SpeculationCandidate::New();
  candidate8->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate8->requires_anonymous_client_ip_when_cross_origin = true;
  candidate8->url = GetSameOriginUrl("/candidate8.html");
  candidate8->referrer = referrer->Clone();
  candidate8->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate8));

  // Process the candidates with the |PrefetchDocumentManager| for the current
  // document.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &GetPrimaryMainFrame());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);

  // Check that the candidates that should be prefetched were sent to
  // |PrefetchService|.
  const auto& prefetch_urls = GetPrefetches();
  ASSERT_EQ(prefetch_urls.size(), 6U);
  EXPECT_EQ(prefetch_urls[0]->GetURL(), GetCrossOriginUrl("/candidate1.html"));
  EXPECT_EQ(prefetch_urls[0]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kEager));
  EXPECT_TRUE(
      prefetch_urls[0]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_EQ(prefetch_urls[1]->GetURL(), GetCrossOriginUrl("/candidate2.html"));
  EXPECT_EQ(prefetch_urls[1]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/false,
                         blink::mojom::SpeculationEagerness::kEager));
  EXPECT_TRUE(
      prefetch_urls[1]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_EQ(prefetch_urls[2]->GetURL(), GetSameOriginUrl("/candidate3.html"));
  EXPECT_EQ(prefetch_urls[2]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/false,
                         blink::mojom::SpeculationEagerness::kEager));
  EXPECT_FALSE(
      prefetch_urls[2]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_EQ(prefetch_urls[3]->GetURL(), GetCrossOriginUrl("/candidate6.html"));
  EXPECT_EQ(prefetch_urls[3]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kConservative));
  EXPECT_TRUE(
      prefetch_urls[3]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_EQ(prefetch_urls[4]->GetURL(),
            GetSameSiteCrossOriginUrl("/candidate7.html"));
  EXPECT_EQ(prefetch_urls[4]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/false,
                         blink::mojom::SpeculationEagerness::kEager));
  EXPECT_FALSE(
      prefetch_urls[4]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  EXPECT_EQ(prefetch_urls[5]->GetURL(), GetSameOriginUrl("/candidate8.html"));
  EXPECT_EQ(prefetch_urls[5]->GetPrefetchType(),
            PrefetchType(/*use_prefetch_proxy=*/true,
                         blink::mojom::SpeculationEagerness::kEager));
  EXPECT_FALSE(
      prefetch_urls[5]->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  // Check that the only remaining entries in candidates are those that
  // shouldn't be prefetched by |PrefetchService|.
  ASSERT_EQ(candidates.size(), 2U);
  EXPECT_EQ(candidates[0]->url, GetCrossOriginUrl("/candidate4.html"));
  EXPECT_EQ(candidates[1]->url, GetCrossOriginUrl("/candidate5.html"));

  // Check IsPrefetchAttemptFailedOrDiscarded method
  // Discarded candidate
  EXPECT_TRUE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate4.html")));
  // URLs that were not processed
  EXPECT_TRUE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetSameOriginUrl("/random_page.html")));
  // Prefetches with no status yet
  EXPECT_FALSE(prefetch_urls[0]->HasPrefetchStatus());
  EXPECT_FALSE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
  // Prefetches with status
  prefetch_urls[0]->SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
  EXPECT_FALSE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
  prefetch_urls[0]->SetPrefetchStatus(
      PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
  EXPECT_TRUE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
  prefetch_urls[0]->SetPrefetchStatus(PrefetchStatus::kPrefetchFailedNetError);
  EXPECT_TRUE(prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
}

}  // namespace
}  // namespace content
