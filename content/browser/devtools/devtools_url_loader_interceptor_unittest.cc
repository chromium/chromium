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
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
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

}  // namespace

// Tests DevToolsURLLoaderInterceptor and DevToolsURLLoaderFactoryProxy,
// verifying request ID uniqueness per process and bad message enforcement.
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
    StartedLoader result;
    result.client = std::make_unique<network::TestURLLoaderClient>();
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";

    base::RunLoop run_loop;
    on_intercepted_ = run_loop.QuitClosure();
    factory->CreateLoaderAndStart(
        result.loader.BindNewPipeAndPassReceiver(), request_id,
        network::mojom::kURLLoadOptionNone, request,
        result.client->CreateRemote(),
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
  base::OnceClosure on_intercepted_;
};

// Verifies that a renderer process attempting to reuse an in-flight request ID
// is terminated via mojo::ReportBadMessage to prevent request hijacking.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateRequestIdFromRendererKillsProcess) {
  constexpr int kRendererProcessId = 42;
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateFactoryForProcess(kRendererProcessId);

  StartedLoader first = StartRequestAndExpectInterception(
      factory.get(), /*request_id=*/1, GURL("http://example.com/first"));
  EXPECT_FALSE(first.interception_id.empty());

  // Second request reusing request_id = 1 from the same process must fail.
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

  EXPECT_EQ("DevTools: Duplicate request ID",
            bad_message_observer.WaitForBadMessage());
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

// Verifies that a renderer attempting to reuse an in-flight request ID for a
// data: scheme request is still terminated via mojo::ReportBadMessage.
TEST_F(DevToolsURLLoaderInterceptorTest,
       DuplicateRequestIdWithDataSchemeFromRendererKillsProcess) {
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

  EXPECT_EQ("DevTools: Duplicate request ID",
            bad_message_observer.WaitForBadMessage());
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

}  // namespace content
