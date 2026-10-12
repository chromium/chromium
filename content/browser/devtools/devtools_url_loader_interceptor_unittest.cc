// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_loader_interceptor.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "content/public/common/child_process_id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/auth.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
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
    patterns.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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

// Verifies that a browser-initiated request (process_id = 0) colliding with an
// in-flight request ID does not crash the browser and is instead forwarded
// directly to the target factory.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateRequestIdFromBrowserProcessForwardsDirectly) {
  constexpr int kBrowserProcessId = 0;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kBrowserProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  // Second request reusing request_id = 1 from the browser process.
  network::ResourceRequest request2;
  request2.url = GURL("http://example.com/second");
  request2.method = "GET";

  mojo::Remote<network::mojom::URLLoader> loader2;
  network::TestURLLoaderClient client2;
  mojo::test::BadMessageObserver bad_message_observer;

  factory->CreateLoaderAndStart(
      loader2.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request2, client2.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // The request must be forwarded to target_factory_ without triggering
  // a bad message or crashing.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_EQ(1, target_factory_.NumPending());
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
  patterns.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns1.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns2.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns1.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns2.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns1.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
  patterns2.emplace_back("*", base::flat_set<blink::mojom::ResourceType>(),
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
