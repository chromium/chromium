// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

using testing::_;

namespace content {

// Test params used for kSignedHTTPExchangePingValidity, which can be
// removed once we're done with crbug.com/952811.
class SignedExchangeLoaderTest : public testing::TestWithParam<bool> {
 public:
  SignedExchangeLoaderTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kSignedHTTPExchangePingValidity);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kSignedHTTPExchangePingValidity);
    }
  }

  ~SignedExchangeLoaderTest() override = default;

 protected:
  class MockURLLoaderClient final : public network::mojom::URLLoaderClient {
   public:
    explicit MockURLLoaderClient(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> receiver)
        : loader_client_receiver_(this, std::move(receiver)) {}
    ~MockURLLoaderClient() override {}

    // network::mojom::URLLoaderClient overrides:
    MOCK_METHOD1(OnReceiveResponse,
                 void(const network::mojom::URLResponseHeadPtr));
    MOCK_METHOD2(OnReceiveRedirect,
                 void(const net::RedirectInfo&,
                      network::mojom::URLResponseHeadPtr));
    MOCK_METHOD3(OnUploadProgress,
                 void(int64_t, int64_t, base::OnceCallback<void()> callback));
    MOCK_METHOD1(OnReceiveCachedMetadata, void(mojo_base::BigBuffer));
    MOCK_METHOD1(OnTransferSizeUpdated, void(int32_t));
    MOCK_METHOD1(OnStartLoadingResponseBody,
                 void(mojo::ScopedDataPipeConsumerHandle));
    MOCK_METHOD1(OnComplete, void(const network::URLLoaderCompletionStatus&));

   private:
    mojo::Receiver<network::mojom::URLLoaderClient> loader_client_receiver_;
    DISALLOW_COPY_AND_ASSIGN(MockURLLoaderClient);
  };

  class MockURLLoader final : public network::mojom::URLLoader {
   public:
    explicit MockURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
        : receiver_(this, std::move(url_loader_receiver)) {}
    ~MockURLLoader() override = default;

    // network::mojom::URLLoader overrides:
    MOCK_METHOD4(FollowRedirect,
                 void(const std::vector<std::string>&,
                      const net::HttpRequestHeaders&,
                      const net::HttpRequestHeaders&,
                      const base::Optional<GURL>&));
    MOCK_METHOD2(SetPriority,
                 void(net::RequestPriority priority,
                      int32_t intra_priority_value));
    MOCK_METHOD0(PauseReadingBodyFromNet, void());
    MOCK_METHOD0(ResumeReadingBodyFromNet, void());

   private:
    mojo::Receiver<network::mojom::URLLoader> receiver_;

    DISALLOW_COPY_AND_ASSIGN(MockURLLoader);
  };

  // Used only when kSignedHTTPExchangePingValidity is enabled.
  class MockValidityPingURLLoaderFactory
      : public network::mojom::URLLoaderFactory {
   public:
    MockValidityPingURLLoaderFactory() = default;
    ~MockValidityPingURLLoaderFactory() override = default;

    void CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& url_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      ASSERT_FALSE(bool{ping_loader_});
      ping_loader_ = std::make_unique<MockURLLoader>(std::move(receiver));
      ping_loader_client_.Bind(std::move(client));
    }
    void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
        override {}

    std::unique_ptr<MockURLLoader> ping_loader_;
    mojo::Remote<network::mojom::URLLoaderClient> ping_loader_client_;
  };

  network::mojom::URLLoaderClient* ping_loader_client() {
    return ping_loader_factory_.ping_loader_client_.get();
  }

  static std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  ThrottlesGetter() {
    return std::vector<std::unique_ptr<blink::URLLoaderThrottle>>();
  }

  scoped_refptr<network::SharedURLLoaderFactory> CreateMockPingLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &ping_loader_factory_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  MockValidityPingURLLoaderFactory ping_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeLoaderTest);
};

TEST_P(SignedExchangeLoaderTest, Simple) {
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client;
  MockURLLoader mock_loader(loader.InitWithNewPipeAndPassReceiver());
  network::mojom::URLLoaderClientEndpointsPtr endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          std::move(loader), loader_client.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  MockURLLoaderClient mock_client(client.InitWithNewPipeAndPassReceiver());

  network::ResourceRequest resource_request;
  resource_request.url = GURL("https://example.com/test.sxg");

  auto response = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nnContent-type: foo/bar\n\n");
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      resource_request.url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL("https://publisher.example.com/"), "text/html", {},
      net::SHA256HashValue({{0x00}}))});

  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(&factory);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader =
      std::make_unique<SignedExchangeLoader>(
          resource_request, std::move(response), std::move(consumer_handle),
          std::move(client), std::move(endpoints),
          network::mojom::kURLLoadOptionNone,
          false /* should_redirect_to_fallback */, nullptr /* devtools_proxy */,
          nullptr /* reporter */, CreateMockPingLoaderFactory(),
          base::BindRepeating(&SignedExchangeLoaderTest::ThrottlesGetter),
          FrameTreeNode::kFrameTreeNodeInvalidId, nullptr /* metric_recorder */,
          std::string() /* accept_langs */,
          false /* keep_entry_for_prefetch_cache */);

  EXPECT_CALL(mock_loader, PauseReadingBodyFromNet());
  signed_exchange_loader->PauseReadingBodyFromNet();

  EXPECT_CALL(mock_loader, ResumeReadingBodyFromNet());
  signed_exchange_loader->ResumeReadingBodyFromNet();

  constexpr int kIntraPriority = 5;
  EXPECT_CALL(mock_loader,
              SetPriority(net::RequestPriority::HIGHEST, kIntraPriority));
  signed_exchange_loader->SetPriority(net::RequestPriority::HIGHEST,
                                      kIntraPriority);

  EXPECT_CALL(mock_client, OnReceiveRedirect(_, _));
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<mojo::DataPipeProducer> producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* raw_producer = producer.get();
  base::RunLoop run_loop;
  raw_producer->Write(
      std::make_unique<mojo::StringDataSource>(
          "Hello, world!", mojo::StringDataSource::AsyncWritingMode::
                               STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
      base::BindOnce([](std::unique_ptr<mojo::DataPipeProducer> producer,
                        base::OnceClosure quit_closure,
                        MojoResult result) { std::move(quit_closure).Run(); },
                     std::move(producer), run_loop.QuitClosure()));

  network::URLLoaderCompletionStatus status;
  loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  base::RunLoop().RunUntilIdle();

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_after_redirect;
  MockURLLoaderClient mock_client_after_redirect(
      client_after_redirect.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(mock_client_after_redirect, OnReceiveResponse(_));

  if (!base::FeatureList::IsEnabled(
          features::kSignedHTTPExchangePingValidity)) {
    run_loop.Run();
    EXPECT_CALL(mock_client_after_redirect, OnStartLoadingResponseBody(_));
    EXPECT_CALL(mock_client_after_redirect, OnComplete(_));
  }

  signed_exchange_loader->ConnectToClient(std::move(client_after_redirect));
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(features::kSignedHTTPExchangePingValidity)) {
    // When kSignedHTTPExchangePingValidity is enabled, the
    // client-after-redirect will be called only after the ping loader returns
    // something.
    ASSERT_TRUE(ping_loader_client());
    EXPECT_CALL(mock_client_after_redirect, OnStartLoadingResponseBody(_));
    EXPECT_CALL(mock_client_after_redirect, OnComplete(_));
    ping_loader_client()->OnReceiveResponse(
        network::mojom::URLResponseHead::New());
    ping_loader_client()->OnComplete(
        network::URLLoaderCompletionStatus(net::OK));
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedExchangeLoaderTest,
                         ::testing::Values(false, true));

}  // namespace content
