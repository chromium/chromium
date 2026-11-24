// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_loader_interceptor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/common/child_process_id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/auth.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeCookieManager : public network::TestCookieManager {
 public:
  void CloneInterface(
      mojo::PendingReceiver<network::mojom::CookieManager> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void GetCookieList(
      const GURL& url,
      const net::CookieOptions& cookie_options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override {
    std::move(callback).Run({}, {});
  }

  void AddReceiver(
      mojo::PendingReceiver<network::mojom::CookieManager> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<network::mojom::CookieManager> receivers_;
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void GetCookieManager(
      mojo::PendingReceiver<network::mojom::CookieManager> receiver) override {
    cookie_manager_.AddReceiver(std::move(receiver));
  }

 private:
  FakeCookieManager cookie_manager_;
};

class TestContinueRequestCallback
    : public DevToolsURLLoaderInterceptor::ContinueInterceptedRequestCallback {
 public:
  void sendSuccess() override {}
  void sendFailure(const protocol::Response& response) override {
    ADD_FAILURE() << "ContinueInterceptedRequest failed: "
                  << response.Message();
  }
  void fallThrough() override {}
};

// Simulates a client-side embedder header client such as extensions'
// WebRequestProxyingURLLoaderFactory: it registers navigation requests under
// their browser-assigned request ID and drops OnLoaderCreated receivers for
// unknown IDs (which causes network::URLLoader to fail with net::ERR_FAILED).
class FakeDownstreamHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient,
      public network::mojom::TrustedHeaderClient {
 public:
  explicit FakeDownstreamHeaderClient(int32_t expected_request_id)
      : expected_request_id_(expected_request_id) {}

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    loader_created_request_ids_.push_back(request_id);
    if (request_id != expected_request_id_) {
      return;
    }
    header_client_receiver_.reset();
    header_client_receiver_.Bind(std::move(receiver));
  }

  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {}

  void OnLoaderFailedWithRemoteEndpoint(
      int32_t request_id,
      const net::IPEndPoint& remote_endpoint) override {
    failed_endpoint_future_.SetValue(request_id, remote_endpoint);
  }

  // network::mojom::TrustedHeaderClient:
  void OnBeforeSendHeaders(const GURL& request_url,
                           const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override {
    ++before_send_headers_count_;
    std::move(callback).Run(net::OK, headers, std::nullopt);
  }

  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& endpoint,
                         const std::optional<net::SSLInfo>& ssl_info,
                         OnHeadersReceivedCallback callback) override {
    std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
  }

  const std::vector<int32_t>& loader_created_request_ids() const {
    return loader_created_request_ids_;
  }
  base::test::TestFuture<int32_t, net::IPEndPoint>& failed_endpoint_future() {
    return failed_endpoint_future_;
  }
  int before_send_headers_count() const { return before_send_headers_count_; }

 private:
  const int32_t expected_request_id_;
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient> receiver_{this};
  mojo::Receiver<network::mojom::TrustedHeaderClient> header_client_receiver_{
      this};
  std::vector<int32_t> loader_created_request_ids_;
  base::test::TestFuture<int32_t, net::IPEndPoint> failed_endpoint_future_;
  int before_send_headers_count_ = 0;
};

}  // namespace

// Tests DevToolsURLLoaderInterceptor and DevToolsURLLoaderFactoryProxy,
// verifying request ID collision handling and restart behavior, and
// auth challenge routing across daisy-chained interceptors.
class DevToolsURLLoaderInterceptorTest : public testing::Test {
 public:
  struct StartedLoader {
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    std::string interception_id;
  };

  DevToolsURLLoaderInterceptorTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    storage_partition_.set_network_context(&network_context_);
    interceptor_ = std::make_unique<DevToolsURLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [this](std::unique_ptr<InterceptedRequestInfo> info) {
              ASSERT_TRUE(info);
              last_interception_id_ = info->interception_id;
              last_intercepted_info_ = std::move(info);
              if (on_intercepted_) {
                std::move(on_intercepted_).Run();
              }
            }));
    std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
    patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                          DevToolsURLLoaderInterceptor::kRequest);
    interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/false);
  }

  void TearDown() override { interceptor_.reset(); }

  mojo::Remote<network::mojom::URLLoaderFactory> CreateFactoryForProcess(
      int process_id) {
    network::mojom::URLLoaderFactoryOverride devtools_override;
    bool created = interceptor_->CreateProxyForInterception(
        process_id, &storage_partition_, base::UnguessableToken::Create(),
        /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
        /*header_client=*/nullptr);
    CHECK(created);
    target_factory_.Clone(
        std::move(devtools_override.overridden_factory_receiver));
    return mojo::Remote<network::mojom::URLLoaderFactory>(
        std::move(devtools_override.overriding_factory));
  }

  StartedLoader StartRequestAndExpectInterception(
      network::mojom::URLLoaderFactory* factory,
      int32_t request_id,
      const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    return StartAndExpectInterception(
        factory, request_id, network::mojom::kURLLoadOptionNone, request);
  }

  // Starts a CORS preflight the way the network service's PreflightController
  // does: an OPTIONS request that reuses the request ID of the request it
  // precedes.
  StartedLoader StartPreflightAndExpectInterception(
      network::mojom::URLLoaderFactory* factory,
      int32_t request_id,
      const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "OPTIONS";
    return StartAndExpectInterception(
        factory, request_id, network::mojom::kURLLoadOptionAsCorsPreflight,
        request);
  }

  // Starts a GET request, continues it past interception, and delivers a 302
  // redirect to `redirect_url`. The returned request waits for
  // FollowRedirect(), which is when the network service sends the redirect's
  // CORS preflight.
  StartedLoader StartRequestAndReceiveRedirect(
      network::mojom::URLLoaderFactory* factory,
      int32_t request_id,
      const GURL& url,
      const GURL& redirect_url) {
    StartedLoader result =
        StartRequestAndExpectInterception(factory, request_id, url);
    interceptor_->ContinueInterceptedRequest(
        result.interception_id,
        std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
        std::make_unique<TestContinueRequestCallback>());
    target_factory_.WaitForRequest(url);
    CHECK_EQ(1, target_factory_.NumPending());

    net::RedirectInfo redirect_info;
    redirect_info.status_code = net::HTTP_FOUND;
    redirect_info.new_method = "GET";
    redirect_info.new_url = redirect_url;
    target_factory_.GetPendingRequest(0)->client->OnReceiveRedirect(
        redirect_info, network::CreateURLResponseHead(net::HTTP_FOUND));
    result.client->RunUntilRedirectReceived();
    return result;
  }

 private:
  StartedLoader StartAndExpectInterception(
      network::mojom::URLLoaderFactory* factory,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request) {
    StartedLoader result;
    result.client = std::make_unique<network::TestURLLoaderClient>();

    base::RunLoop run_loop;
    on_intercepted_ = run_loop.QuitClosure();
    factory->CreateLoaderAndStart(
        result.loader.BindNewPipeAndPassReceiver(), request_id, options,
        request, result.client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    run_loop.Run();

    result.interception_id = last_interception_id_;
    return result;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  FakeNetworkContext network_context_;
  TestStoragePartition storage_partition_;
  network::TestURLLoaderFactory target_factory_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> interceptor_;
  std::string last_interception_id_;
  std::unique_ptr<InterceptedRequestInfo> last_intercepted_info_;
  base::OnceClosure on_intercepted_;
};

// Verifies that a browser-initiated request using a renderer process ID
// (such as a dedicated worker main script fetch via WorkerScriptFetcher)
// with a negative request ID succeeds and is intercepted without error.
TEST_F(DevToolsURLLoaderInterceptorTest,
       WorkerMainScriptFetchWithNegativeRequestIdSucceeds) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader loader = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/-2, GURL("http://example.com/worker.js"));

  EXPECT_FALSE(loader.interception_id.empty());
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  interceptor_->ContinueInterceptedRequest(
      loader.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  target_factory_.WaitForRequest(GURL("http://example.com/worker.js"));
  target_factory_.SimulateResponseForPendingRequest(
      "http://example.com/worker.js", "console.log('worker');");
  loader.client->RunUntilComplete();

  EXPECT_EQ(net::OK, loader.client->completion_status().error_code);
}

// Verifies that when a request ID collides with an in-flight request (such
// as a colliding request reusing the negative request ID of an in-flight worker
// script fetch, as in crbug.com/497350668), the previous job is immediately
// shut down upon collision, and subsequent auth challenges route to the new
// request.
TEST_F(DevToolsURLLoaderInterceptorTest,
       CollidingRequestIdShutsDownPreviousJobAndRoutesAuthToNewJob) {
  constexpr int kRendererProcessId = 42;

  // Configure interceptor to handle auth challenges.
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
  patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                        DevToolsURLLoaderInterceptor::kRequest);
  interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/true);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  // 1. First request initiated with a negative request ID (e.g. worker fetch).
  StartedLoader first_loader = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/-2,
      GURL("http://example.com/victim_worker.js"));
  EXPECT_FALSE(first_loader.interception_id.empty());

  mojo::test::BadMessageObserver bad_message_observer;
  // 2. Second request reuses request_id = -2.
  StartedLoader colliding_loader = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/-2, GURL("http://attacker.com/exploit"));

  EXPECT_FALSE(colliding_loader.interception_id.empty());
  EXPECT_NE(first_loader.interception_id, colliding_loader.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // Regression check 1 (b/497350668): In the vulnerability, the previous job
  // remained active in the interceptor map. Verifying that the first client
  // disconnects proves that existing_job->Shutdown() immediately tore down
  // the previous loader upon collision.
  first_loader.client->RunUntilDisconnect();

  // Forward the colliding request to the network so its job enters
  // kRequestSent.
  interceptor_->ContinueInterceptedRequest(
      colliding_loader.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  // Simulate an HTTP auth challenge for the collided GlobalRequestID.
  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "attacker_realm";

  base::RunLoop auth_loop;
  on_intercepted_ = auth_loop.QuitClosure();

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kRendererProcessId),
                      /*request_id=*/-2),
      challenge, base::DoNothing());

  auth_loop.Run();

  // Regression check 2 (b/497350668): In the vulnerability, HandleAuthRequest()
  // routed the auth challenge to the previous job rather than the new request.
  // Verifying that the challenge is delivered to colliding_loader proves that
  // auth is not redirected through to the previous job.
  ASSERT_TRUE(last_intercepted_info_);
  EXPECT_EQ(colliding_loader.interception_id,
            last_intercepted_info_->interception_id);
  EXPECT_NE(first_loader.interception_id,
            last_intercepted_info_->interception_id);
  ASSERT_TRUE(last_intercepted_info_->auth_challenge);
  EXPECT_EQ("attacker_realm", last_intercepted_info_->auth_challenge->realm);
}

// Verifies that when a renderer restarts a request (e.g. Critical Client Hints
// or cross-scheme redirects) with the same request ID while the previous loader
// is still registered, the previous job is cleanly shut down and the new
// request succeeds and completes.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateRequestIdFromRendererShutsDownExistingJobAndSucceeds) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  mojo::test::BadMessageObserver bad_message_observer;
  // Second request reusing request_id = 1 from the same process.
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/second"));

  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // The first loader must be disconnected when its job was shut down.
  first.client->RunUntilDisconnect();

  // Continue the second request and verify end-to-end completion.
  interceptor_->ContinueInterceptedRequest(
      second.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  target_factory_.WaitForRequest(GURL("http://example.com/second"));
  target_factory_.SimulateResponseForPendingRequest("http://example.com/second",
                                                    "second response body");
  second.client->RunUntilComplete();

  EXPECT_EQ(net::OK, second.client->completion_status().error_code);
}

// Verifies that distinct request IDs from the same process both succeed.
TEST_F(DevToolsURLLoaderInterceptorTest, DistinctRequestIdSucceeds) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/2, GURL("http://example.com/second"));

  EXPECT_FALSE(first.interception_id.empty());
  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// Verifies that distinct renderer processes can concurrently use the same
// request_id because GlobalRequestID scopes requests per process.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DistinctProcessIdsWithSameRequestIdSucceed) {
  constexpr int kRenderer1ProcessId = 42;
  constexpr int kRenderer2ProcessId = 43;
  mojo::Remote<network::mojom::URLLoaderFactory> factory1 =
      CreateFactoryForProcess(kRenderer1ProcessId);
  mojo::Remote<network::mojom::URLLoaderFactory> factory2 =
      CreateFactoryForProcess(kRenderer2ProcessId);

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader first = StartRequestAndExpectInterception(
      factory1.get(), /*request_id=*/1, GURL("http://example.com/proc1"));
  StartedLoader second = StartRequestAndExpectInterception(
      factory2.get(), /*request_id=*/1, GURL("http://example.com/proc2"));

  EXPECT_FALSE(first.interception_id.empty());
  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// Verifies that reusing a request ID after the previous request has completed
// succeeds without error.
TEST_F(DevToolsURLLoaderInterceptorTest,
       ReusingRequestIdAfterCompletionSucceeds) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  // Close the first loader and wait for the job to shut down.
  first.loader.reset();
  first.client->RunUntilDisconnect();

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/second"));

  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// Verifies that when a renderer restarts a request to a data: scheme with
// the same request ID while the previous loader is still registered, the
// previous job is shut down and the data request forwards directly to the
// target factory without error.
TEST_F(
    DevToolsURLLoaderInterceptorTest,
    DuplicateRequestIdWithDataSchemeFromRendererShutsDownExistingJobAndSucceeds) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  network::ResourceRequest data_request;
  data_request.url = GURL("data:text/plain,hello");
  data_request.method = "GET";

  mojo::Remote<network::mojom::URLLoader> loader2;
  network::TestURLLoaderClient client2;
  mojo::test::BadMessageObserver bad_message_observer;

  factory->CreateLoaderAndStart(
      loader2.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, data_request, client2.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  target_factory_.WaitForRequest(GURL("data:text/plain,hello"));
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_EQ(1, target_factory_.NumPending());

  // The first loader must be disconnected when its job was shut down.
  first.client->RunUntilDisconnect();

  // Simulate completion of the data URL and verify it completes cleanly.
  target_factory_.SimulateResponseForPendingRequest("data:text/plain,hello",
                                                    "hello");
  client2.RunUntilComplete();
  EXPECT_EQ(net::OK, client2.completion_status().error_code);
}

// Verifies that when a browser-initiated request (process_id = 0) restarts
// with the same non-zero request ID (e.g. NavigationURLLoaderImpl reusing
// global_request_id_) while the previous loader is still registered, the
// obsolete job is shut down and the restarted request is intercepted and
// forwarded under the original request ID.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateRequestIdFromBrowserProcessShutsDownExistingJobAndPreservesId) {
  constexpr int kBrowserProcessId = 0;
  constexpr int32_t kNavigationRequestId = -10;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), kNavigationRequestId, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), kNavigationRequestId, GURL("http://example.com/second"));

  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // The obsolete job must be shut down upon collision.
  first.client->RunUntilDisconnect();

  // The restarted request must be sent to the target factory under the
  // original request ID and complete cleanly.
  interceptor_->ContinueInterceptedRequest(
      second.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  target_factory_.WaitForRequest(GURL("http://example.com/second"));
  ASSERT_EQ(1, target_factory_.NumPending());
  EXPECT_EQ(kNavigationRequestId,
            target_factory_.GetPendingRequest(0)->request_id);

  target_factory_.SimulateResponseForPendingRequest("http://example.com/second",
                                                    "second response body");
  second.client->RunUntilComplete();
  EXPECT_EQ(net::OK, second.client->completion_status().error_code);
}

// Verifies that when a browser-initiated request (process_id = 0) restarts
// with the same non-zero request ID while the previous job is in kRequestSent,
// the previous job is shut down and subsequent auth challenges for that ID
// route to the restarted job under the original request ID.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateNonZeroRequestIdFromBrowserProcessRoutesAuthToNewJob) {
  constexpr int kBrowserProcessId = 0;
  constexpr int32_t kNavigationRequestId = -10;

  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
  patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                        DevToolsURLLoaderInterceptor::kRequest);
  interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/true);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), kNavigationRequestId, GURL("http://example.com/first"));
  ASSERT_FALSE(first.interception_id.empty());

  interceptor_->ContinueInterceptedRequest(
      first.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://example.com/first"));
  ASSERT_EQ(1, target_factory_.NumPending());
  EXPECT_EQ(kNavigationRequestId,
            target_factory_.GetPendingRequest(0)->request_id);

  // Restart the request with the same ID while the first job is in
  // kRequestSent.
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), kNavigationRequestId, GURL("http://other.test/second"));
  ASSERT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  first.client->RunUntilDisconnect();

  interceptor_->ContinueInterceptedRequest(
      second.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://other.test/second"));
  // Shutting down the first job disconnects its upstream network request,
  // leaving only the restarted request connected under the original ID.
  EXPECT_EQ(1, target_factory_.NumPending());
  ASSERT_EQ(2u, target_factory_.pending_requests()->size());
  EXPECT_FALSE(target_factory_.GetPendingRequest(0)->client.is_connected());
  EXPECT_TRUE(target_factory_.GetPendingRequest(1)->client.is_connected());
  EXPECT_EQ(GURL("http://other.test/second"),
            target_factory_.GetPendingRequest(1)->request.url);
  EXPECT_EQ(kNavigationRequestId,
            target_factory_.GetPendingRequest(1)->request_id);

  // An auth challenge for the original ID must route to the restarted job, not
  // the obsolete first job.
  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "second_realm";

  base::RunLoop auth_loop;
  on_intercepted_ = auth_loop.QuitClosure();
  base::test::TestFuture<bool, std::optional<net::AuthCredentials>> auth_result;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kBrowserProcessId),
                      kNavigationRequestId),
      challenge,
      auth_result
          .GetCallback<bool, const std::optional<net::AuthCredentials>&>());

  ASSERT_FALSE(auth_result.IsReady());
  auth_loop.Run();
  ASSERT_TRUE(last_intercepted_info_);
  EXPECT_EQ(second.interception_id, last_intercepted_info_->interception_id);
  ASSERT_TRUE(last_intercepted_info_->network_request);
  EXPECT_EQ("http://other.test/second",
            last_intercepted_info_->network_request->GetUrl());
  ASSERT_TRUE(last_intercepted_info_->auth_challenge);
  EXPECT_EQ("second_realm", last_intercepted_info_->auth_challenge->realm);

  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"second_user", u"second_pass");
  interceptor_->ContinueInterceptedRequest(
      second.interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());

  ASSERT_TRUE(auth_result.Wait());
  EXPECT_FALSE(auth_result.Get<0>());
  ASSERT_TRUE(auth_result.Get<1>().has_value());
  EXPECT_EQ(u"second_user", auth_result.Get<1>()->username());
  EXPECT_EQ(u"second_pass", auth_result.Get<1>()->password());
}

// Verifies that when a navigation restarts with the same browser-assigned
// request ID and a downstream TrustedURLLoaderHeaderClient (such as
// extensions' WebRequestProxyingURLLoaderFactory) is attached, OnLoaderCreated
// and OnLoaderFailedWithRemoteEndpoint for the restarted request are forwarded
// to the downstream header client under the original request ID without
// disconnecting the header client pipe.
TEST_F(DevToolsURLLoaderInterceptorTest,
       NavigationRestartPreservesRequestIdForDownstreamHeaderClient) {
  constexpr int kRendererProcessId = 42;
  constexpr int32_t kNavigationRequestId = -10;

  FakeDownstreamHeaderClient downstream_header_client(kNavigationRequestId);
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client = downstream_header_client.BindNewPipeAndPassRemote();

  network::mojom::URLLoaderFactoryOverride devtools_override;
  ASSERT_TRUE(interceptor_->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, base::UnguessableToken::Create(),
      /*is_navigation=*/true, /*is_download=*/false, &devtools_override,
      &header_client));
  ASSERT_TRUE(header_client.is_valid());

  target_factory_.Clone(
      std::move(devtools_override.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> factory(
      std::move(devtools_override.overriding_factory));
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>
      proxy_header_client(std::move(header_client));

  // Start the initial navigation request and let it reach the network.
  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), kNavigationRequestId, GURL("http://example.com/first"));
  interceptor_->ContinueInterceptedRequest(
      first.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://example.com/first"));
  ASSERT_EQ(1, target_factory_.NumPending());
  EXPECT_EQ(kNavigationRequestId,
            target_factory_.GetPendingRequest(0)->request_id);

  // Network service notifies OnLoaderCreated for the initial loader.
  mojo::Remote<network::mojom::TrustedHeaderClient> loader1_header_client;
  proxy_header_client->OnLoaderCreated(
      kNavigationRequestId, loader1_header_client.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                           std::optional<base::DictValue>>
        before_send_future;
    loader1_header_client->OnBeforeSendHeaders(
        GURL("http://example.com/first"), net::HttpRequestHeaders(),
        before_send_future
            .GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
    ASSERT_TRUE(before_send_future.Wait());
    EXPECT_EQ(net::OK, before_send_future.Get<0>());
  }

  // Restart the navigation with the same request ID while the initial job is
  // still registered.
  StartedLoader restarted =
      StartRequestAndExpectInterception(factory.get(), kNavigationRequestId,
                                        GURL("http://example.com/restarted"));
  first.client->RunUntilDisconnect();

  interceptor_->ContinueInterceptedRequest(
      restarted.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://example.com/restarted"));
  // Shutting down the initial job disconnects its upstream network request,
  // leaving only the restarted request connected under the original ID.
  EXPECT_EQ(1, target_factory_.NumPending());
  ASSERT_EQ(2u, target_factory_.pending_requests()->size());
  EXPECT_FALSE(target_factory_.GetPendingRequest(0)->client.is_connected());
  EXPECT_TRUE(target_factory_.GetPendingRequest(1)->client.is_connected());
  EXPECT_EQ(GURL("http://example.com/restarted"),
            target_factory_.GetPendingRequest(1)->request.url);
  const int32_t restarted_network_request_id =
      target_factory_.GetPendingRequest(1)->request_id;
  EXPECT_EQ(kNavigationRequestId, restarted_network_request_id);

  // Network service notifies OnLoaderCreated using the request ID it received
  // for the restarted loader. If DevTools had rewritten the ID, the downstream
  // header client would not recognize it and would drop the receiver.
  mojo::Remote<network::mojom::TrustedHeaderClient> loader2_header_client;
  proxy_header_client->OnLoaderCreated(
      restarted_network_request_id,
      loader2_header_client.BindNewPipeAndPassReceiver());
  bool loader2_header_client_disconnected = false;
  loader2_header_client.set_disconnect_handler(base::BindLambdaForTesting(
      [&]() { loader2_header_client_disconnected = true; }));

  {
    base::test::TestFuture<int32_t, std::optional<net::HttpRequestHeaders>,
                           std::optional<base::DictValue>>
        before_send_future;
    loader2_header_client->OnBeforeSendHeaders(
        GURL("http://example.com/restarted"), net::HttpRequestHeaders(),
        before_send_future
            .GetCallback<int32_t, const std::optional<net::HttpRequestHeaders>&,
                         std::optional<base::DictValue>>());
    ASSERT_TRUE(before_send_future.Wait());
    EXPECT_EQ(net::OK, before_send_future.Get<0>());
  }
  EXPECT_FALSE(loader2_header_client_disconnected);
  EXPECT_EQ(2, downstream_header_client.before_send_headers_count());
  ASSERT_EQ(2u, downstream_header_client.loader_created_request_ids().size());
  EXPECT_EQ(kNavigationRequestId,
            downstream_header_client.loader_created_request_ids()[0]);
  EXPECT_EQ(kNavigationRequestId,
            downstream_header_client.loader_created_request_ids()[1]);

  // Verify OnLoaderFailedWithRemoteEndpoint also preserves the original ID.
  const net::IPEndPoint kRemoteEndpoint(net::IPAddress(192, 0, 2, 1), 443);
  proxy_header_client->OnLoaderFailedWithRemoteEndpoint(
      restarted_network_request_id, kRemoteEndpoint);
  ASSERT_TRUE(downstream_header_client.failed_endpoint_future().Wait());
  EXPECT_EQ(kNavigationRequestId,
            downstream_header_client.failed_endpoint_future().Get<0>());
  EXPECT_EQ(kRemoteEndpoint,
            downstream_header_client.failed_endpoint_future().Get<1>());
}

// Verifies that concurrent browser-initiated requests passing request_id = 0
// (such as concurrent downloads started by ResourceDownloader) are assigned
// distinct browser-initiated request IDs, are both intercepted, and route auth
// challenges to their respective jobs without cross-routing.
TEST_F(DevToolsURLLoaderInterceptorTest,
       ConcurrentBrowserRequestsWithZeroRequestIdAreInterceptedAndIsolateAuth) {
  constexpr int kBrowserProcessId = 0;

  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
  patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                        DevToolsURLLoaderInterceptor::kRequest);
  interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/true);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);

  // Start first download-like request with request_id = 0 and continue it to
  // the network so its job is in State::kRequestSent.
  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/0, GURL("http://example.com/first.bin"));
  ASSERT_FALSE(first.interception_id.empty());

  interceptor_->ContinueInterceptedRequest(
      first.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://example.com/first.bin"));
  ASSERT_EQ(1, target_factory_.NumPending());
  const int32_t first_network_req_id =
      target_factory_.GetPendingRequest(0)->request_id;
  EXPECT_LT(first_network_req_id, 0);

  // Start second concurrent request with request_id = 0 while the first is
  // still awaiting response headers.
  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/0, GURL("http://other.test/second.bin"));
  ASSERT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);
  EXPECT_FALSE(first.client->has_received_disconnect());

  interceptor_->ContinueInterceptedRequest(
      second.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(GURL("http://other.test/second.bin"));
  ASSERT_EQ(2, target_factory_.NumPending());
  const int32_t second_network_req_id =
      target_factory_.GetPendingRequest(1)->request_id;
  EXPECT_LT(second_network_req_id, 0);
  EXPECT_NE(first_network_req_id, second_network_req_id);

  // Simulate an auth challenge arriving for the second request's network ID.
  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "second_realm";

  base::RunLoop auth_loop;
  on_intercepted_ = auth_loop.QuitClosure();

  bool handle_auth_completed = false;
  bool fallback_result = true;
  std::optional<net::AuthCredentials> creds_result;
  base::RunLoop finish_auth_loop;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kBrowserProcessId),
                      second_network_req_id),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
            finish_auth_loop.Quit();
          }));

  auth_loop.Run();
  ASSERT_TRUE(last_intercepted_info_);
  EXPECT_EQ(second.interception_id, last_intercepted_info_->interception_id);
  EXPECT_NE(first.interception_id, last_intercepted_info_->interception_id);
  ASSERT_TRUE(last_intercepted_info_->network_request);
  EXPECT_EQ("http://other.test/second.bin",
            last_intercepted_info_->network_request->GetUrl());
  ASSERT_TRUE(last_intercepted_info_->auth_challenge);
  EXPECT_EQ("second_realm", last_intercepted_info_->auth_challenge->realm);

  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"test_user", u"test_pass");
  interceptor_->ContinueInterceptedRequest(
      second.interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());

  finish_auth_loop.Run();
  EXPECT_TRUE(handle_auth_completed);
  EXPECT_FALSE(fallback_result);
  ASSERT_TRUE(creds_result.has_value());
  EXPECT_EQ(u"test_user", creds_result->username());
  EXPECT_EQ(u"test_pass", creds_result->password());

  // Complete the first request normally and verify it was unaffected.
  target_factory_.SimulateResponseForPendingRequest(
      "http://example.com/first.bin", "first body");
  first.client->RunUntilComplete();
  EXPECT_EQ(net::OK, first.client->completion_status().error_code);
}

// Verifies that browser-initiated requests that carry no request ID of their
// own (request_id = 0, as issued for downloads) are each intercepted as a
// separate job instead of colliding on GlobalRequestID(browser, 0) and
// bypassing interception.
TEST_F(DevToolsURLLoaderInterceptorTest,
       ConcurrentBrowserRequestsWithoutRequestIdAreInterceptedSeparately) {
  constexpr int kBrowserProcessId = 0;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/0, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  StartedLoader second = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/0, GURL("http://example.com/second"));
  EXPECT_FALSE(second.interception_id.empty());
  EXPECT_NE(first.interception_id, second.interception_id);

  // Neither request reached the network before the client resolved it.
  EXPECT_EQ(0, target_factory_.NumPending());
}

// Verifies that an auth challenge the network service reports for the request
// ID shared by browser-initiated requests without an ID of their own
// (GlobalRequestID(browser, 0)) is never handed to an InterceptionJob. Such a
// challenge can belong to any of those requests, including ones this
// interceptor never saw, so answering it on behalf of a job would send the
// credentials the client holds for that job's URL to a different server
// (crbug.com/574234398). The job stays reachable through its own unique ID.
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeForSharedBrowserRequestIdIsNotRoutedToJob) {
  constexpr int kBrowserProcessId = 0;
  const GURL kUrl("http://example.com/download");

  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
  patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                        DevToolsURLLoaderInterceptor::kRequest);
  interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/true);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);
  StartedLoader started =
      StartRequestAndExpectInterception(factory.get(), /*request_id=*/0, kUrl);
  interceptor_->ContinueInterceptedRequest(
      started.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kUrl);
  ASSERT_EQ(1, target_factory_.NumPending());

  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  // A challenge keyed by the shared ID falls back to default handling right
  // away. No job takes it, so it is never shown to the client.
  base::test::TestFuture<bool, std::optional<net::AuthCredentials>>
      shared_id_result;
  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kBrowserProcessId),
                      /*request_id=*/0),
      challenge,
      shared_id_result
          .GetCallback<bool, const std::optional<net::AuthCredentials>&>());
  ASSERT_TRUE(shared_id_result.IsReady());
  EXPECT_TRUE(shared_id_result.Get<0>());
  EXPECT_FALSE(shared_id_result.Get<1>().has_value());

  // The job sent its request to the network under a unique ID, and a challenge
  // for that ID is still routed to it.
  const int32_t network_request_id =
      target_factory_.GetPendingRequest(0)->request_id;
  ASSERT_NE(0, network_request_id);

  base::RunLoop auth_loop;
  on_intercepted_ = auth_loop.QuitClosure();
  base::test::TestFuture<bool, std::optional<net::AuthCredentials>>
      own_id_result;
  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kBrowserProcessId),
                      network_request_id),
      challenge,
      own_id_result
          .GetCallback<bool, const std::optional<net::AuthCredentials>&>());
  ASSERT_FALSE(own_id_result.IsReady());
  auth_loop.Run();
  ASSERT_TRUE(last_intercepted_info_);
  ASSERT_TRUE(last_intercepted_info_->auth_challenge);
  EXPECT_EQ("realm", last_intercepted_info_->auth_challenge->realm);
  EXPECT_EQ(started.interception_id, last_intercepted_info_->interception_id);

  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"test_user", u"test_pass");
  interceptor_->ContinueInterceptedRequest(
      last_intercepted_info_->interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());
  ASSERT_TRUE(own_id_result.Wait());
  EXPECT_FALSE(own_id_result.Get<0>());
  ASSERT_TRUE(own_id_result.Get<1>().has_value());
  EXPECT_EQ(own_id_result.Get<1>()->username(), u"test_user");
  EXPECT_EQ(own_id_result.Get<1>()->password(), u"test_pass");
}

// Verifies that when two unrelated interceptors (e.g. DevTools sessions
// attached to two different pages) each intercept a download, an auth challenge
// for one download is routed to the interceptor that owns it. Downloads are
// issued with request_id = 0 through a proxy created with is_navigation = true,
// so without a unique ID per request both jobs would be registered for auth
// routing under GlobalRequestID(browser, 0) and the challenge would go to
// whichever job registered last (crbug.com/574234398).
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeForDownloadRoutesToOwningInterceptor) {
  constexpr int kRendererProcessId = 42;
  constexpr int kBrowserProcessId = 0;
  const GURL kUrl1("http://one.example/download");
  const GURL kUrl2("http://two.example/download");

  std::unique_ptr<InterceptedRequestInfo> info1;
  std::unique_ptr<InterceptedRequestInfo> auth_info1;
  std::unique_ptr<InterceptedRequestInfo> info2;
  std::unique_ptr<InterceptedRequestInfo> auth_info2;
  base::RunLoop loop1;
  base::RunLoop loop2;
  // Quits when either interceptor is asked to handle an auth challenge.
  base::RunLoop auth_loop;

  auto interceptor1 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            if (info->auth_challenge) {
              auth_info1 = std::move(info);
              auth_loop.Quit();
            } else {
              info1 = std::move(info);
              loop1.Quit();
            }
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns1;
  patterns1.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor1->SetPatterns(std::move(patterns1), /*handle_auth=*/true);

  auto interceptor2 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            if (info->auth_challenge) {
              auth_info2 = std::move(info);
              auth_loop.Quit();
            } else {
              info2 = std::move(info);
              loop2.Quit();
            }
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns2;
  patterns2.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor2->SetPatterns(std::move(patterns2), /*handle_auth=*/true);

  // Each interceptor installs its own download proxy; they are not chained.
  network::mojom::URLLoaderFactoryOverride override1;
  ASSERT_TRUE(interceptor1->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, base::UnguessableToken::Create(),
      /*is_navigation=*/true, /*is_download=*/true, &override1,
      /*header_client=*/nullptr));
  target_factory_.Clone(std::move(override1.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> factory1(
      std::move(override1.overriding_factory));

  network::mojom::URLLoaderFactoryOverride override2;
  ASSERT_TRUE(interceptor2->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, base::UnguessableToken::Create(),
      /*is_navigation=*/true, /*is_download=*/true, &override2,
      /*header_client=*/nullptr));
  target_factory_.Clone(std::move(override2.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> factory2(
      std::move(override2.overriding_factory));

  // Start both downloads the way ResourceDownloader does, with request_id = 0,
  // and let both reach the network.
  network::ResourceRequest request1;
  request1.url = kUrl1;
  request1.method = "GET";
  mojo::Remote<network::mojom::URLLoader> loader1;
  network::TestURLLoaderClient client1;
  factory1->CreateLoaderAndStart(
      loader1.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request1, client1.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  loop1.Run();
  ASSERT_TRUE(info1);
  interceptor1->ContinueInterceptedRequest(
      info1->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kUrl1);

  network::ResourceRequest request2;
  request2.url = kUrl2;
  request2.method = "GET";
  mojo::Remote<network::mojom::URLLoader> loader2;
  network::TestURLLoaderClient client2;
  factory2->CreateLoaderAndStart(
      loader2.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionNone, request2, client2.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  loop2.Run();
  ASSERT_TRUE(info2);
  interceptor2->ContinueInterceptedRequest(
      info2->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kUrl2);
  ASSERT_EQ(2, target_factory_.NumPending());

  // The network service identifies the request an auth challenge belongs to by
  // the request ID the request was sent with.
  std::optional<int32_t> network_request_id1;
  for (const auto& pending : *target_factory_.pending_requests()) {
    if (pending.request.url == kUrl1) {
      network_request_id1 = pending.request_id;
    }
  }
  ASSERT_TRUE(network_request_id1.has_value());

  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  base::test::TestFuture<bool, std::optional<net::AuthCredentials>> result;
  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kBrowserProcessId),
                      *network_request_id1),
      challenge,
      result.GetCallback<bool, const std::optional<net::AuthCredentials>&>());
  // The challenge is pending with a job rather than already resolved.
  ASSERT_FALSE(result.IsReady());
  auth_loop.Run();

  // Only the interceptor that owns the first download is asked, and it is
  // asked about that download.
  ASSERT_TRUE(auth_info1);
  EXPECT_FALSE(auth_info2);
  EXPECT_EQ(info1->interception_id, auth_info1->interception_id);

  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"test_user", u"test_pass");
  interceptor1->ContinueInterceptedRequest(
      auth_info1->interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());
  ASSERT_TRUE(result.Wait());
  EXPECT_FALSE(result.Get<0>());
  ASSERT_TRUE(result.Get<1>().has_value());
  EXPECT_EQ(result.Get<1>()->username(), u"test_user");
  EXPECT_EQ(result.Get<1>()->password(), u"test_pass");

  loader1.reset();
  loader2.reset();
  client1.RunUntilDisconnect();
  client2.RunUntilDisconnect();
}

// Verifies that a CORS preflight sharing the request ID of an in-flight request
// does not abort that request. With kAvoidCorsURLLoaderRestartOnRedirect, the
// network service's CorsURLLoader keeps the original loader alive across a
// redirect and sends the preflight for the redirect target through the DevTools
// factory override with the same request ID, so both loaders are legitimately
// in flight at the same time (crbug.com/570354835).
TEST_F(DevToolsURLLoaderInterceptorTest,
       CorsPreflightForRedirectDoesNotAbortInFlightRequest) {
  constexpr int kRendererProcessId = 42;
  constexpr int32_t kRequestId = 1;
  const GURL kOriginalUrl("http://cross-origin.test/manifest.json?x=1");
  const GURL kRedirectUrl("http://cross-origin.test/manifest.json");
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader main_request = StartRequestAndReceiveRedirect(
      factory.get(), kRequestId, kOriginalUrl, kRedirectUrl);

  mojo::test::BadMessageObserver bad_message_observer;
  StartedLoader preflight = StartPreflightAndExpectInterception(
      factory.get(), kRequestId, kRedirectUrl);
  EXPECT_NE(main_request.interception_id, preflight.interception_id);

  // The main request must survive the preflight. If it is aborted,
  // FollowRedirect() below waits forever for an interception.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_FALSE(main_request.client->has_received_completion());
  ASSERT_FALSE(main_request.client->has_received_disconnect());

  // The preflight completes.
  interceptor_->ContinueInterceptedRequest(
      preflight.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kRedirectUrl);
  ASSERT_TRUE(target_factory_.SimulateResponseForPendingRequest(
      kRedirectUrl.spec(), "", net::HTTP_NO_CONTENT));
  preflight.client->RunUntilComplete();
  EXPECT_EQ(net::OK, preflight.client->completion_status().error_code);

  // The main request then follows the redirect and is intercepted again as the
  // next hop of the same request.
  base::RunLoop redirect_intercepted;
  on_intercepted_ = redirect_intercepted.QuitClosure();
  main_request.loader->FollowRedirect({}, std::nullopt);
  redirect_intercepted.Run();
  ASSERT_TRUE(last_intercepted_info_);
  ASSERT_TRUE(last_intercepted_info_->redirected_request_id);
  EXPECT_EQ(main_request.interception_id,
            *last_intercepted_info_->redirected_request_id);
}

// Verifies that when a CORS preflight sharing the request ID of an in-flight
// request shuts down, the in-flight request stays indexed by its
// GlobalRequestID. A later non-preflight request with the same ID must still
// collide with, and shut down, the in-flight request.
TEST_F(DevToolsURLLoaderInterceptorTest,
       CompletedCorsPreflightKeepsInFlightRequestIndexed) {
  constexpr int kRendererProcessId = 42;
  constexpr int32_t kRequestId = 1;
  const GURL kOriginalUrl("http://cross-origin.test/manifest.json?x=1");
  const GURL kRedirectUrl("http://cross-origin.test/manifest.json");
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader main_request = StartRequestAndReceiveRedirect(
      factory.get(), kRequestId, kOriginalUrl, kRedirectUrl);

  // The preflight completes and its job shuts down, which calls RemoveJob()
  // with the shared GlobalRequestID.
  StartedLoader preflight = StartPreflightAndExpectInterception(
      factory.get(), kRequestId, kRedirectUrl);
  interceptor_->ContinueInterceptedRequest(
      preflight.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kRedirectUrl);
  ASSERT_TRUE(target_factory_.SimulateResponseForPendingRequest(
      kRedirectUrl.spec(), "", net::HTTP_NO_CONTENT));
  preflight.client->RunUntilComplete();
  preflight.loader.reset();
  preflight.client->RunUntilDisconnect();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(main_request.client->has_received_disconnect());

  // A non-preflight request reusing the ID collides with the main request.
  StartedLoader colliding = StartRequestAndExpectInterception(
      factory.get(), kRequestId, GURL("http://cross-origin.test/other"));
  main_request.client->RunUntilDisconnect();
  EXPECT_FALSE(colliding.client->has_received_disconnect());
}

// Verifies that an auth challenge for a request ID shared by an in-flight
// request and its CORS preflight is never routed to the preflight job.
// Preflights omit credentials, so a challenge for the shared ID must not be
// answered by the preflight's interception.
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeNotRoutedToCorsPreflightJob) {
  constexpr int kRendererProcessId = 42;
  constexpr int32_t kRequestId = 1;
  const GURL kOriginalUrl("http://cross-origin.test/manifest.json?x=1");
  const GURL kRedirectUrl("http://cross-origin.test/manifest.json");

  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns;
  patterns.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                        DevToolsURLLoaderInterceptor::kRequest);
  interceptor_->SetPatterns(std::move(patterns), /*handle_auth=*/true);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  // The main request waits for FollowRedirect(), so it cannot handle auth.
  StartedLoader main_request = StartRequestAndReceiveRedirect(
      factory.get(), kRequestId, kOriginalUrl, kRedirectUrl);

  // The preflight is sent to the network, which is the state in which a job
  // handles auth challenges.
  StartedLoader preflight = StartPreflightAndExpectInterception(
      factory.get(), kRequestId, kRedirectUrl);
  interceptor_->ContinueInterceptedRequest(
      preflight.interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());
  target_factory_.WaitForRequest(kRedirectUrl);

  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  bool handle_auth_completed = false;
  bool fallback_result = false;
  std::optional<net::AuthCredentials> creds_result;
  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kRendererProcessId),
                      kRequestId),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
          }));

  // No job can handle the challenge, so it falls back to default handling
  // synchronously.
  EXPECT_TRUE(handle_auth_completed);
  EXPECT_TRUE(fallback_result);
  EXPECT_FALSE(creds_result.has_value());
  EXPECT_FALSE(main_request.client->has_received_disconnect());
}

// Verifies that in a daisy-chained interceptor setup, an auth challenge is
// routed to the interceptor configured to handle auth (handle_auth = true),
// bypassing any interceptor that does not handle auth.
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeRoutesToInterceptorHandlingAuth) {
  constexpr int kRendererProcessId = 42;
  const base::UnguessableToken frame_token = base::UnguessableToken::Create();

  std::unique_ptr<InterceptedRequestInfo> info1;
  base::RunLoop loop1;
  auto interceptor1 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            info1 = std::move(info);
            loop1.Quit();
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns1;
  patterns1.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor1->SetPatterns(std::move(patterns1), /*handle_auth=*/false);

  std::unique_ptr<InterceptedRequestInfo> info2;
  std::unique_ptr<InterceptedRequestInfo> auth_info2;
  base::RunLoop loop2;
  base::RunLoop auth_loop;
  auto interceptor2 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            if (info->auth_challenge) {
              auth_info2 = std::move(info);
              auth_loop.Quit();
            } else {
              info2 = std::move(info);
              loop2.Quit();
            }
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns2;
  patterns2.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor2->SetPatterns(std::move(patterns2), /*handle_auth=*/true);

  // Chain interceptor1 then interceptor2. Because each proxy wraps the
  // previous override's target receiver, interceptor1 becomes the outermost
  // proxy (client-facing) and interceptor2 becomes the innermost proxy
  // (network-facing).
  network::mojom::URLLoaderFactoryOverride devtools_override;
  bool created1 = interceptor1->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created1);

  bool created2 = interceptor2->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created2);

  target_factory_.Clone(
      std::move(devtools_override.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> client_factory(
      std::move(devtools_override.overriding_factory));

  // Start request.
  network::ResourceRequest request;
  request.url = GURL("http://example.com/auth");
  request.method = "GET";
  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;

  client_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Interceptor 1 intercepts request first.
  loop1.Run();
  ASSERT_TRUE(info1);
  interceptor1->ContinueInterceptedRequest(
      info1->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  // Interceptor 2 intercepts request next.
  loop2.Run();
  ASSERT_TRUE(info2);
  interceptor2->ContinueInterceptedRequest(
      info2->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  // Both jobs forwarded the request to the network. Simulate auth challenge.
  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  bool handle_auth_completed = false;
  bool fallback_result = false;
  std::optional<net::AuthCredentials> creds_result;
  base::RunLoop finish_loop;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kRendererProcessId),
                      /*request_id=*/1),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
            finish_loop.Quit();
          }));

  // Interceptor 2 handles auth, so it receives the auth challenge.
  auth_loop.Run();
  ASSERT_TRUE(auth_info2);
  ASSERT_TRUE(auth_info2->auth_challenge);
  EXPECT_EQ(auth_info2->auth_challenge->realm, "realm");
  EXPECT_FALSE(handle_auth_completed);

  // Provide credentials from Interceptor 2.
  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"test_user", u"test_pass");
  interceptor2->ContinueInterceptedRequest(
      auth_info2->interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());

  finish_loop.Run();
  EXPECT_TRUE(handle_auth_completed);
  EXPECT_FALSE(fallback_result);
  ASSERT_TRUE(creds_result.has_value());
  EXPECT_EQ(creds_result->username(), u"test_user");
  EXPECT_EQ(creds_result->password(), u"test_pass");

  loader.reset();
  client.RunUntilDisconnect();
}

// Verifies that when no interceptor handles auth, HandleAuthRequest falls back
// to default authentication handling without error.
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeFallsBackWhenNoInterceptorHandlesAuth) {
  constexpr int kRendererProcessId = 42;
  const base::UnguessableToken frame_token = base::UnguessableToken::Create();

  std::unique_ptr<InterceptedRequestInfo> info1;
  base::RunLoop loop1;
  auto interceptor1 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            info1 = std::move(info);
            loop1.Quit();
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns1;
  patterns1.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor1->SetPatterns(std::move(patterns1), /*handle_auth=*/false);

  std::unique_ptr<InterceptedRequestInfo> info2;
  base::RunLoop loop2;
  auto interceptor2 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            info2 = std::move(info);
            loop2.Quit();
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns2;
  patterns2.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor2->SetPatterns(std::move(patterns2), /*handle_auth=*/false);

  // Chain interceptor1 then interceptor2.
  network::mojom::URLLoaderFactoryOverride devtools_override;
  bool created1 = interceptor1->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created1);

  bool created2 = interceptor2->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created2);

  target_factory_.Clone(
      std::move(devtools_override.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> client_factory(
      std::move(devtools_override.overriding_factory));

  network::ResourceRequest request;
  request.url = GURL("http://example.com/noauth");
  request.method = "GET";
  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;

  client_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  loop1.Run();
  ASSERT_TRUE(info1);
  interceptor1->ContinueInterceptedRequest(
      info1->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  loop2.Run();
  ASSERT_TRUE(info2);
  interceptor2->ContinueInterceptedRequest(
      info2->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  bool handle_auth_completed = false;
  bool fallback_result = false;
  std::optional<net::AuthCredentials> creds_result;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kRendererProcessId),
                      /*request_id=*/1),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
          }));

  EXPECT_TRUE(handle_auth_completed);
  EXPECT_TRUE(fallback_result);
  EXPECT_FALSE(creds_result.has_value());
  EXPECT_FALSE(info1->auth_challenge);
  EXPECT_FALSE(info2->auth_challenge);

  loader.reset();
  client.RunUntilDisconnect();
}

// Verifies that HandleAuthRequest falls back to default handling when no
// in-flight job exists for the given GlobalRequestID.
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeFallsBackWhenNoInFlightJob) {
  constexpr int kUnknownProcessId = 99;
  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  bool handle_auth_completed = false;
  bool fallback_result = false;
  std::optional<net::AuthCredentials> creds_result;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kUnknownProcessId),
                      /*request_id=*/1),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
          }));

  EXPECT_TRUE(handle_auth_completed);
  EXPECT_TRUE(fallback_result);
  EXPECT_FALSE(creds_result.has_value());
}

// Verifies that when multiple interceptors handle auth, the challenge routes to
// the innermost interceptor (closest to the network, top of stack).
TEST_F(DevToolsURLLoaderInterceptorTest,
       AuthChallengeRoutesToInnermostInterceptorHandlingAuth) {
  constexpr int kRendererProcessId = 42;
  const base::UnguessableToken frame_token = base::UnguessableToken::Create();

  std::unique_ptr<InterceptedRequestInfo> info1;
  std::unique_ptr<InterceptedRequestInfo> auth_info1;
  base::RunLoop loop1;
  auto interceptor1 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            if (info->auth_challenge) {
              auth_info1 = std::move(info);
            } else {
              info1 = std::move(info);
              loop1.Quit();
            }
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns1;
  patterns1.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor1->SetPatterns(std::move(patterns1), /*handle_auth=*/true);

  std::unique_ptr<InterceptedRequestInfo> info2;
  std::unique_ptr<InterceptedRequestInfo> auth_info2;
  base::RunLoop loop2;
  base::RunLoop auth_loop;
  auto interceptor2 =
      std::make_unique<DevToolsURLLoaderInterceptor>(base::BindLambdaForTesting(
          [&](std::unique_ptr<InterceptedRequestInfo> info) {
            if (info->auth_challenge) {
              auth_info2 = std::move(info);
              auth_loop.Quit();
            } else {
              info2 = std::move(info);
              loop2.Quit();
            }
          }));
  std::vector<DevToolsURLLoaderInterceptor::Pattern> patterns2;
  patterns2.emplace_back("*", base::EnumSet<blink::mojom::ResourceType>(),
                         DevToolsURLLoaderInterceptor::kRequest);
  interceptor2->SetPatterns(std::move(patterns2), /*handle_auth=*/true);

  // Chain interceptor1 then interceptor2. Because each proxy wraps the
  // previous override's target receiver, interceptor1 becomes the outermost
  // proxy (client-facing) and interceptor2 becomes the innermost proxy
  // (network-facing).
  network::mojom::URLLoaderFactoryOverride devtools_override;
  bool created1 = interceptor1->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created1);

  bool created2 = interceptor2->CreateProxyForInterception(
      kRendererProcessId, &storage_partition_, frame_token,
      /*is_navigation=*/false, /*is_download=*/false, &devtools_override,
      /*header_client=*/nullptr);
  ASSERT_TRUE(created2);

  target_factory_.Clone(
      std::move(devtools_override.overridden_factory_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> client_factory(
      std::move(devtools_override.overriding_factory));

  network::ResourceRequest request;
  request.url = GURL("http://example.com/bothauth");
  request.method = "GET";
  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;

  client_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  loop1.Run();
  ASSERT_TRUE(info1);
  interceptor1->ContinueInterceptedRequest(
      info1->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  loop2.Run();
  ASSERT_TRUE(info2);
  interceptor2->ContinueInterceptedRequest(
      info2->interception_id,
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(),
      std::make_unique<TestContinueRequestCallback>());

  net::AuthChallengeInfo challenge;
  challenge.scheme = "basic";
  challenge.realm = "realm";

  bool handle_auth_completed = false;
  bool fallback_result = false;
  std::optional<net::AuthCredentials> creds_result;
  base::RunLoop finish_loop;

  DevToolsURLLoaderInterceptor::HandleAuthRequest(
      GlobalRequestID(ToOriginatingProcessIdUnsafe(kRendererProcessId),
                      /*request_id=*/1),
      challenge,
      base::BindLambdaForTesting(
          [&](bool use_fallback,
              const std::optional<net::AuthCredentials>& creds) {
            handle_auth_completed = true;
            fallback_result = use_fallback;
            creds_result = creds;
            finish_loop.Quit();
          }));

  // Interceptor 2 (innermost, top of stack) handles the challenge.
  auth_loop.Run();
  ASSERT_TRUE(auth_info2);
  ASSERT_TRUE(auth_info2->auth_challenge);
  EXPECT_EQ(auth_info2->auth_challenge->realm, "realm");
  EXPECT_EQ(auth_info1, nullptr);
  EXPECT_FALSE(handle_auth_completed);

  // Provide credentials from Interceptor 2.
  auto mods = std::make_unique<DevToolsURLLoaderInterceptor::Modifications>();
  mods->auth_challenge_response =
      std::make_unique<DevToolsURLLoaderInterceptor::AuthChallengeResponse>(
          u"inner_user", u"inner_pass");
  interceptor2->ContinueInterceptedRequest(
      auth_info2->interception_id, std::move(mods),
      std::make_unique<TestContinueRequestCallback>());

  finish_loop.Run();
  EXPECT_TRUE(handle_auth_completed);
  EXPECT_FALSE(fallback_result);
  ASSERT_TRUE(creds_result.has_value());
  EXPECT_EQ(creds_result->username(), u"inner_user");
  EXPECT_EQ(creds_result->password(), u"inner_pass");

  loader.reset();
  client.RunUntilDisconnect();
}

}  // namespace content
