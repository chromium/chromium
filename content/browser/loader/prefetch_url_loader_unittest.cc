// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

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

}  // namespace
}  // namespace content
