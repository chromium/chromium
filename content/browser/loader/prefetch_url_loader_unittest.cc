// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/loader/prefetch_url_loader_service_context.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class ScopedBadMessageCounter {
 public:
  ScopedBadMessageCounter() {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &ScopedBadMessageCounter::OnBadMessage, base::Unretained(this)));
  }
  ~ScopedBadMessageCounter() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  ScopedBadMessageCounter(const ScopedBadMessageCounter&) = delete;
  ScopedBadMessageCounter& operator=(const ScopedBadMessageCounter&) = delete;

  int count() const { return count_; }
  const std::string& last_error() const { return last_error_; }

 private:
  void OnBadMessage(const std::string& error) {
    ++count_;
    last_error_ = error;
  }

  int count_ = 0;
  std::string last_error_;
};

class PrefetchURLLoaderTest : public RenderViewHostTestHarness {
 protected:
  PrefetchURLLoaderTest() = default;
  ~PrefetchURLLoaderTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(PrefetchURLLoaderTest, RedirectIsolationInfoUpdate) {
  // Initialize same-site resource request.
  network::ResourceRequest request;
  request.url = GURL("https://a.test/redirect");
  request.method = "GET";
  request.load_flags = net::LOAD_PREFETCH;

  url::Origin referring_origin = url::Origin::Create(GURL("https://a.test"));
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, referring_origin,
      referring_origin, net::SiteForCookies());

  net::NetworkAnonymizationKey initial_nak =
      request.trusted_params->isolation_info.network_anonymization_key();

  network::TestURLLoaderClient forwarding_client;

  auto prefetch_url_loader = std::make_unique<PrefetchURLLoader>(
      /*request_id=*/0,
      /*options=*/0,
      /*frame_tree_node_id=*/main_rfh()->GetFrameTreeNodeId(), request,
      initial_nak, forwarding_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      test_shared_url_loader_factory_, base::BindRepeating([]() {
        return std::vector<std::unique_ptr<blink::URLLoaderThrottle>>();
      }),
      browser_context(),
      /*prefetched_signed_exchange_cache=*/nullptr,
      /*accept_langs=*/"", base::BindOnce([](const network::ResourceRequest&) {
        return base::UnguessableToken::Create();
      }));

  // Verify initial request URL and IsolationInfo.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_req = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_req);
  EXPECT_EQ(pending_req->request.url, GURL("https://a.test/redirect"));

  ASSERT_TRUE(pending_req->request.trusted_params.has_value());
  EXPECT_EQ(
      pending_req->request.trusted_params->isolation_info.top_frame_origin(),
      referring_origin);
  EXPECT_EQ(pending_req->request.trusted_params->isolation_info.frame_origin(),
            referring_origin);

  // Simulate redirect to cross-site target.
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://b.test/target");

  network::mojom::URLResponseHeadPtr redirect_head =
      network::mojom::URLResponseHead::New();
  redirect_head->headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "302 Found")
          .AddHeader("Location", redirect_info.new_url.spec())
          .Build();

  pending_req->client->OnReceiveRedirect(redirect_info,
                                         std::move(redirect_head));
  forwarding_client.RunUntilRedirectReceived();

  // Verify updated IsolationInfo.
  const network::ResourceRequest& updated_request =
      prefetch_url_loader->resource_request_for_testing();
  EXPECT_EQ(updated_request.url, GURL("https://b.test/target"));
  ASSERT_TRUE(updated_request.trusted_params.has_value());

  url::Origin expected_target_origin =
      url::Origin::Create(GURL("https://b.test"));

  EXPECT_EQ(updated_request.trusted_params->isolation_info.top_frame_origin(),
            expected_target_origin);
  EXPECT_EQ(updated_request.trusted_params->isolation_info.frame_origin(),
            expected_target_origin);

  // Verify updated NAK matches cross-site target NAK.
  net::NetworkAnonymizationKey expected_nak =
      updated_request.trusted_params->isolation_info
          .network_anonymization_key();

  EXPECT_EQ(prefetch_url_loader->network_anonymization_key_for_testing(),
            expected_nak);
}

// Verifies that a cross-origin prefetch arriving before navigation commit
// fails validation with net::ERR_INVALID_ARGUMENT without killing the renderer
// or caching a factory for the pre-commit origin, and that subsequent
// prefetches succeed once committed.
TEST_F(PrefetchURLLoaderTest,
       CrossOriginPrefetchPreCommitOriginMismatchDoesNotKillOrPoisonFactory) {
  ScopedBadMessageCounter bad_messages;
  SubresourceProxyingURLLoaderService service(browser_context());

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://sub1.a.test/page1.html"));

  // Start a navigation to https://sub2.a.test/page2.html and advance to
  // ReadyToCommit(). This picks the final RenderFrameHostImpl (`target_rfh`, a
  // speculative RFH whose `GetLastCommittedOrigin()` is still opaque) and
  // binds the SubresourceProxyingURLLoaderService factory before
  // DidCommitProvisionalLoad runs.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://sub2.a.test/page2.html"), web_contents());
  navigation->ReadyToCommit();
  TestRenderFrameHost* target_rfh =
      static_cast<TestRenderFrameHost*>(navigation->GetFinalRenderFrameHost());
  ASSERT_TRUE(target_rfh);

  const url::Origin new_origin =
      url::Origin::Create(GURL("https://sub2.a.test"));
  ASSERT_NE(target_rfh->GetLastCommittedOrigin(), new_origin);

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      service.GetFactory(factory.BindNewPipeAndPassReceiver(),
                         target_rfh->GetFrameTreeNodeId(),
                         test_shared_url_loader_factory_,
                         target_rfh->GetWeakPtr(),
                         /*prefetched_signed_exchange_cache=*/nullptr);
  ASSERT_TRUE(bind_context);
  EXPECT_FALSE(bind_context->did_commit_navigation);

  network::ResourceRequest request;
  request.url = GURL("https://c.test/target.html");
  request.method = "GET";
  request.load_flags =
      net::LOAD_PREFETCH | net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  request.request_initiator = new_origin;

  network::TestURLLoaderClient client1;
  mojo::Remote<network::mojom::URLLoader> loader1;
  factory->CreateLoaderAndStart(
      loader1.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client1.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client1.RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client1.completion_status().error_code);
  EXPECT_EQ(0, bad_messages.count()) << bad_messages.last_error();
  ASSERT_TRUE(bind_context);
  EXPECT_EQ(bind_context->cross_origin_factory, nullptr);

  // Finish committing https://sub2.a.test/page2.html on `target_rfh` and
  // notify `bind_context`. A subsequent cross-origin prefetch on the same
  // factory pipe should now pass validation and initialize
  // `cross_origin_factory`.
  navigation->Commit();
  ASSERT_EQ(main_rfh(), target_rfh);
  ASSERT_EQ(target_rfh->GetLastCommittedOrigin(), new_origin);
  bind_context->OnDidCommitNavigation(target_rfh->GetWeakDocumentPtr());

  bool prefetch_loader_created = false;
  service.prefetch_url_loader_service_context_for_testing()
      .RegisterPrefetchLoaderCallbackForTest(base::BindRepeating(
          [](bool* created) { *created = true; }, &prefetch_loader_created));

  network::TestURLLoaderClient client2;
  mojo::Remote<network::mojom::URLLoader> loader2;
  factory->CreateLoaderAndStart(
      loader2.BindNewPipeAndPassReceiver(), /*request_id=*/2,
      network::mojom::kURLLoadOptionNone, request, client2.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  factory.FlushForTesting();

  EXPECT_EQ(0, bad_messages.count()) << bad_messages.last_error();
  EXPECT_TRUE(prefetch_loader_created);
  ASSERT_TRUE(bind_context);
  EXPECT_NE(bind_context->cross_origin_factory, nullptr);
}

// Verifies that in-flight prefetches from a document that has navigated away
// on the same RenderFrameHost abort with net::ERR_ABORTED without killing the
// renderer.
TEST_F(PrefetchURLLoaderTest,
       CrossOriginPrefetchAbortedAfterDocumentNavigatesAwayOnSameRFH) {
  ScopedBadMessageCounter bad_messages;
  SubresourceProxyingURLLoaderService service(browser_context());

  TestRenderFrameHost* rfh = static_cast<TestRenderFrameHost*>(main_rfh());
  const url::Origin origin_a = url::Origin::Create(GURL("https://sub1.a.test"));
  rfh->SetLastCommittedOriginForTesting(origin_a);

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      service.GetFactory(factory.BindNewPipeAndPassReceiver(),
                         rfh->GetFrameTreeNodeId(),
                         test_shared_url_loader_factory_, rfh->GetWeakPtr(),
                         /*prefetched_signed_exchange_cache=*/nullptr);
  ASSERT_TRUE(bind_context);
  bind_context->OnDidCommitNavigation(rfh->GetWeakDocumentPtr());
  EXPECT_TRUE(bind_context->did_commit_navigation);
  EXPECT_EQ(bind_context->document.AsRenderFrameHostIfValid(), rfh);

  // Commit a cross-document navigation to https://sub2.a.test/page2.html on the
  // same RenderFrameHostImpl (`has_committed_any_navigation()` is false prior
  // to this commit, so `rfh` is reused under default features). This
  // invalidates the previous document's WeakDocumentPtr and updates
  // `GetLastCommittedOrigin()` to https://sub2.a.test.
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://sub2.a.test/page2.html"));
  ASSERT_EQ(main_rfh(), rfh);
  ASSERT_TRUE(bind_context->render_frame_host);
  EXPECT_EQ(bind_context->render_frame_host->GetLastCommittedOrigin(),
            url::Origin::Create(GURL("https://sub2.a.test")));
  EXPECT_TRUE(bind_context->did_commit_navigation);
  EXPECT_EQ(bind_context->document.AsRenderFrameHostIfValid(), nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://c.test/target.html");
  request.method = "GET";
  request.load_flags =
      net::LOAD_PREFETCH | net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  request.request_initiator = origin_a;

  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
  EXPECT_EQ(0, bad_messages.count()) << bad_messages.last_error();
  ASSERT_TRUE(bind_context);
  EXPECT_EQ(bind_context->cross_origin_factory, nullptr);
}

}  // namespace
}  // namespace content
