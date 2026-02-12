// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetcher.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetcherTest : public RenderViewHostTestHarness {
 public:
  PrefetcherTest() = default;
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    SetContents(TestWebContents::Create(
        GetBrowserContext(), SiteInstanceImpl::Create(GetBrowserContext())));
    NavigateAndCommit(GetSameOriginUrl("/"));

    prefetch_service_ =
        std::make_unique<TestPrefetchService>(GetBrowserContext());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }
  void TearDown() override {
    // The PrefetchService we created for the test contains a
    // PrefetchOriginProber, which holds a raw pointer to the BrowserContext.
    // When tearing down, it's important to free our PrefetchService
    // before freeing the BrowserContext, to avoid any chance of a use after
    // free.
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    prefetch_service_.reset();

    DeleteContents();
    RenderViewHostTestHarness::TearDown();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  TestPrefetchService* GetPrefetchService() { return prefetch_service_.get(); }

 private:
  std::unique_ptr<TestPrefetchService> prefetch_service_;
};

TEST_F(PrefetcherTest, ProcessCandidatesForPrefetch) {
  auto prefetcher = Prefetcher(*main_rfh());

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = blink::mojom::Referrer::New();
  candidates.push_back(std::move(candidate1));

  prefetcher.ProcessCandidatesForPrefetch(candidates);
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  EXPECT_FALSE(prefetcher.IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
  GetPrefetchService()->prefetches_[0]->SetPrefetchStatus(
      PrefetchStatus::kPrefetchFailedNetError);
  EXPECT_TRUE(prefetcher.IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
}

}  // namespace
}  // namespace content
