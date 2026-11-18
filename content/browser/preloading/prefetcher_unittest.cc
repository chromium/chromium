// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetcher.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class PrefetcherTest : public RenderViewHostTestHarness {
 public:
  PrefetcherTest() = default;
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    // Set `TestPrefetchService` as the `PrefetchService` of
    // `GetBrowserContext()` before anything (e.g. navigation) creates the
    // default `PrefetchService`.
    BrowserContextImpl::From(GetBrowserContext())
        ->SetPrefetchServiceForTesting(
            std::make_unique<TestPrefetchService>(GetBrowserContext()));

    SetContents(TestWebContents::Create(
        GetBrowserContext(), SiteInstanceImpl::Create(GetBrowserContext())));
    NavigateAndCommit(GetSameOriginUrl("/"));
  }
  void TearDown() override {
    DeleteContents();
    RenderViewHostTestHarness::TearDown();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  TestPrefetchService* GetPrefetchService() {
    return static_cast<TestPrefetchService*>(
        BrowserContextImpl::From(GetBrowserContext())->GetPrefetchService());
  }
};

TEST_F(PrefetcherTest, ProcessCandidatesForPrefetch) {
  auto prefetcher = Prefetcher(*main_rfh());

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->tags = {std::nullopt};
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
