// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void PrefetchUrl(
      base::WeakPtr<PrefetchContainer> prefetch_container) override {
    prefetches_.push_back(prefetch_container);
  }

  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_;
};

class PrefetchDocumentManagerTest : public RenderViewHostTestHarness {
 public:
  PrefetchDocumentManagerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
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

  RenderFrameHostImpl& GetMainFrame() {
    return web_contents_->GetPrimaryPage().GetMainDocument();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  const std::vector<base::WeakPtr<PrefetchContainer>>& GetPrefetches() {
    return prefetch_service_->prefetches_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<TestPrefetchService> prefetch_service_;
};

TEST_F(PrefetchDocumentManagerTest, ProcessSpeculationCandidates) {
  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  // Create candidate for private cross-origin prefetch. This candidate should
  // be prefetched by |PrefetchDocumentManager|.
  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidates.push_back(std::move(candidate1));

  // Create candidate for non-private cross-origin prefetch. This candidate
  // should be prefetched by |PrefetchDocumentManager|.
  auto candidate2 = blink::mojom::SpeculationCandidate::New();
  candidate2->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate2->requires_anonymous_client_ip_when_cross_origin = false;
  candidate2->url = GetCrossOriginUrl("/candidate2.html");
  candidates.push_back(std::move(candidate2));

  // Create candidate for non-private cross-origin prefetch. This candidate
  // should be prefetched by |PrefetchDocumentManager|.
  auto candidate3 = blink::mojom::SpeculationCandidate::New();
  candidate3->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate3->requires_anonymous_client_ip_when_cross_origin = false;
  candidate3->url = GetSameOriginUrl("/candidate3.html");
  candidates.push_back(std::move(candidate3));

  // Create candidate for private cross-origin prefetch with subresources. This
  // candidate should not be prefetched by |PrefetchDocumentManager|.
  auto candidate4 = blink::mojom::SpeculationCandidate::New();
  candidate4->action =
      blink::mojom::SpeculationAction::kPrefetchWithSubresources;
  candidate4->requires_anonymous_client_ip_when_cross_origin = true;
  candidate4->url = GetCrossOriginUrl("/candidate4.html");
  candidates.push_back(std::move(candidate4));

  // Create candidate for prerender. This candidate should not be prefetched by
  // |PrefetchDocumentManager|.
  auto candidate5 = blink::mojom::SpeculationCandidate::New();
  candidate5->action = blink::mojom::SpeculationAction::kPrerender;
  candidate5->requires_anonymous_client_ip_when_cross_origin = false;
  candidate5->url = GetCrossOriginUrl("/candidate5.html");
  candidates.push_back(std::move(candidate5));

  // Process the candidates with the |PrefetchDocumentManager| for the current
  // document.
  PrefetchDocumentManager::GetOrCreateForCurrentDocument(&GetMainFrame())
      ->ProcessCandidates(candidates);

  // Check that the candidates that should be prefetched were sent to
  // |PrefetchService|.
  const auto& prefetch_urls = GetPrefetches();
  ASSERT_EQ(prefetch_urls.size(), 3U);
  EXPECT_EQ(prefetch_urls[0]->GetURL(), GetCrossOriginUrl("/candidate1.html"));
  EXPECT_EQ(prefetch_urls[0]->GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/true,
                         /*use_prefetch_proxy=*/true));
  EXPECT_EQ(prefetch_urls[1]->GetURL(), GetCrossOriginUrl("/candidate2.html"));
  EXPECT_EQ(prefetch_urls[1]->GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/true,
                         /*use_prefetch_proxy=*/false));
  EXPECT_EQ(prefetch_urls[2]->GetURL(), GetSameOriginUrl("/candidate3.html"));
  EXPECT_EQ(prefetch_urls[2]->GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/false,
                         /*use_prefetch_proxy=*/false));

  // Check that the only remaining entries in candidates are those that
  // shouldn't be prefetched by |PrefetchService|.
  ASSERT_EQ(candidates.size(), 2U);
  EXPECT_EQ(candidates[0]->url, GetCrossOriginUrl("/candidate4.html"));
  EXPECT_EQ(candidates[1]->url, GetCrossOriginUrl("/candidate5.html"));
}

}  // namespace
}  // namespace content
