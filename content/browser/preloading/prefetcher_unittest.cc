// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetcher.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MockSpeculationHostDelegate : public SpeculationHostDelegate {
 public:
  explicit MockSpeculationHostDelegate(RenderFrameHost& render_frame_host) {}
  ~MockSpeculationHostDelegate() override = default;

  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) override {
    for (auto&& candidate : candidates) {
      candidates_.push_back(std::move(candidate));
    }
  }
  std::vector<blink::mojom::SpeculationCandidatePtr>& Candidates() {
    return candidates_;
  }

  base::WeakPtr<MockSpeculationHostDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates_;
  base::WeakPtrFactory<MockSpeculationHostDelegate> weak_ptr_factory_{this};
};

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void PrefetchUrl(
      base::WeakPtr<PrefetchContainer> prefetch_container) override {
    prefetch_container->DisablePrecogLoggingForTest();
    prefetches_.push_back(prefetch_container);

    const auto& devtools_observer = prefetch_container->GetDevToolsObserver();
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    network::ResourceRequest::TrustedParams trusted_params;
    request->trusted_params = trusted_params;
    request->trusted_params->devtools_observer =
        devtools_observer->MakeSelfOwnedNetworkServiceDevToolsObserver();
    devtools_observer->OnStartSinglePrefetch(prefetch_container->RequestId(),
                                             *request, std::nullopt);

    network::mojom::URLResponseHead response_head;
    devtools_observer->OnPrefetchResponseReceived(
        prefetch_container->GetURL(), prefetch_container->RequestId(),
        response_head);

    devtools_observer->OnPrefetchRequestComplete(
        prefetch_container->RequestId(),
        network::URLLoaderCompletionStatus{net::ERR_NOT_IMPLEMENTED});

    devtools_observer->OnPrefetchBodyDataReceived(
        prefetch_container->RequestId(), /*body*/ std::string{},
        /*is_base64_encoded=*/false);
  }

  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_;
};

class MockContentBrowserClient : public TestContentBrowserClient {
 public:
  MockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }
  ~MockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  std::unique_ptr<SpeculationHostDelegate> CreateSpeculationHostDelegate(
      RenderFrameHost& render_frame_host) override {
    auto delegate =
        std::make_unique<MockSpeculationHostDelegate>(render_frame_host);
    delegate_ = delegate->AsWeakPtr();
    return delegate;
  }

  base::WeakPtr<MockSpeculationHostDelegate> GetDelegate() { return delegate_; }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
  base::WeakPtr<MockSpeculationHostDelegate> delegate_;
};

class PrefetcherTest : public RenderViewHostTestHarness {
 public:
  PrefetcherTest() = default;
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->NavigateAndCommit(GetSameOriginUrl("/"));
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

    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& GetPrimaryMainFrame() {
    return web_contents_->GetPrimaryPage().GetMainDocument();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  TestPrefetchService* GetPrefetchService() { return prefetch_service_.get(); }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<TestPrefetchService> prefetch_service_;
};

TEST_F(PrefetcherTest, ProcessCandidatesForPrefetch) {
  MockContentBrowserClient browser_client;
  auto prefetcher = Prefetcher(GetPrimaryMainFrame());
  base::WeakPtr<MockSpeculationHostDelegate> delegate =
      browser_client.GetDelegate();
  ASSERT_TRUE(delegate);

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = blink::mojom::Referrer::New();
  candidates.push_back(std::move(candidate1));

  prefetcher.ProcessCandidatesForPrefetch(candidates);
  EXPECT_TRUE(delegate->Candidates().empty());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  EXPECT_FALSE(prefetcher.IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
  GetPrefetchService()->prefetches_[0]->SetPrefetchStatus(
      PrefetchStatus::kPrefetchFailedNetError);
  EXPECT_TRUE(prefetcher.IsPrefetchAttemptFailedOrDiscarded(
      GetCrossOriginUrl("/candidate1.html")));
}

class MockPrefetcher : public Prefetcher {
 public:
  explicit MockPrefetcher(RenderFrameHost& render_frame_host)
      : Prefetcher(render_frame_host) {}

  void OnStartSinglePrefetch(
      const std::string& request_id,
      const network::ResourceRequest& request,
      std::optional<
          std::pair<const GURL&,
                    const network::mojom::URLResponseHeadDevToolsInfo&>>
          redirect_info) override {
    on_start_signle_prefetch_was_called_ = true;
    dev_tools_observer_is_valid_ =
        request.trusted_params->devtools_observer.is_valid();
  }
  void OnPrefetchResponseReceived(
      const GURL& url,
      const std::string& request_id,
      const network::mojom::URLResponseHead& response) override {
    on_prefetch_response_received_was_called_ = true;
  }
  void OnPrefetchRequestComplete(
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status) override {
    on_prefetch_request_complete_was_called_ = true;
  }
  void OnPrefetchBodyDataReceived(const std::string& request_id,
                                  const std::string& body,
                                  bool is_base64_encoded) override {
    on_prefetch_body_data_received_was_called_ = true;
  }

  bool on_start_signle_prefetch_was_called_ = false;
  bool on_prefetch_response_received_was_called_ = false;
  bool on_prefetch_request_complete_was_called_ = false;
  bool on_prefetch_body_data_received_was_called_ = false;
  bool dev_tools_observer_is_valid_ = false;
};

TEST_F(PrefetcherTest, MockPrefetcher) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPrefetchUseContentRefactor,
      {{"proxy_host", "https://testproxyhost.com"}});

  MockContentBrowserClient browser_client;
  auto prefetcher = MockPrefetcher(GetPrimaryMainFrame());
  base::WeakPtr<MockSpeculationHostDelegate> delegate =
      browser_client.GetDelegate();
  ASSERT_TRUE(delegate);

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = blink::mojom::Referrer::New();
  candidates.push_back(std::move(candidate1));

  prefetcher.ProcessCandidatesForPrefetch(candidates);
  EXPECT_TRUE(delegate->Candidates().empty());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  EXPECT_TRUE(prefetcher.on_start_signle_prefetch_was_called_);
  EXPECT_TRUE(prefetcher.on_prefetch_response_received_was_called_);
  EXPECT_TRUE(prefetcher.on_prefetch_request_complete_was_called_);
  EXPECT_TRUE(prefetcher.on_prefetch_body_data_received_was_called_);
  EXPECT_TRUE(prefetcher.dev_tools_observer_is_valid_);
}

}  // namespace
}  // namespace content
