// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class PrefetchDocumentManagerTest : public RenderViewHostTestHarness {
 public:
  PrefetchDocumentManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        content::features::kPrefetchUseContentRefactor);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->NavigateAndCommit(GetSameOriginUrl("/"));
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl* GetMainFrame() { return web_contents_->GetMainFrame(); }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
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
  PrefetchDocumentManager::GetOrCreateForCurrentDocument(GetMainFrame())
      ->ProcessCandidates(candidates);

  // Check that the candidates that should be prefetched were removed, and the
  // others were kept.
  ASSERT_EQ(candidates.size(), 2U);
  EXPECT_EQ(candidates[0]->url, GetCrossOriginUrl("/candidate4.html"));
  EXPECT_EQ(candidates[1]->url, GetCrossOriginUrl("/candidate5.html"));
}

}  // namespace
}  // namespace content
