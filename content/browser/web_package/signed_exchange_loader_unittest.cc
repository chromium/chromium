// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

class SignedExchangeLoaderTest : public testing::Test {
 public:
  SignedExchangeLoaderTest() = default;
  SignedExchangeLoaderTest(const SignedExchangeLoaderTest&) = delete;
  SignedExchangeLoaderTest& operator=(const SignedExchangeLoaderTest&) = delete;

  ~SignedExchangeLoaderTest() override = default;

 protected:
  class MockURLLoaderClient final : public network::mojom::URLLoaderClient {
   public:
    explicit MockURLLoaderClient(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> receiver)
        : loader_client_receiver_(this, std::move(receiver)) {}

    MockURLLoaderClient(const MockURLLoaderClient&) = delete;
    MockURLLoaderClient& operator=(const MockURLLoaderClient&) = delete;

    ~MockURLLoaderClient() override {}

    // network::mojom::URLLoaderClient overrides:
    MOCK_METHOD1(OnReceiveEarlyHints,
                 void(const network::mojom::EarlyHintsPtr));
    MOCK_METHOD3(OnReceiveResponse,
                 void(const network::mojom::URLResponseHeadPtr,
                      mojo::ScopedDataPipeConsumerHandle,
                      std::optional<mojo_base::BigBuffer>));
    MOCK_METHOD2(OnReceiveRedirect,
                 void(const net::RedirectInfo&,
                      network::mojom::URLResponseHeadPtr));
    MOCK_METHOD3(OnUploadProgress,
                 void(int64_t, int64_t, base::OnceCallback<void()> callback));
    MOCK_METHOD1(OnTransferSizeUpdated, void(int32_t));
    MOCK_METHOD1(OnComplete, void(const network::URLLoaderCompletionStatus&));

   private:
    mojo::Receiver<network::mojom::URLLoaderClient> loader_client_receiver_;
  };

  class MockURLLoader final : public network::mojom::URLLoader {
   public:
    explicit MockURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
        : receiver_(this, std::move(url_loader_receiver)) {}

    MockURLLoader(const MockURLLoader&) = delete;
    MockURLLoader& operator=(const MockURLLoader&) = delete;

    ~MockURLLoader() override = default;

    // network::mojom::URLLoader overrides:
    MOCK_METHOD4(FollowRedirect,
                 void(const std::vector<std::string>&,
                      const net::HttpRequestHeaders&,
                      const net::HttpRequestHeaders&,
                      const std::optional<GURL>&));
    MOCK_METHOD2(SetPriority,
                 void(net::RequestPriority priority,
                      int32_t intra_priority_value));
    MOCK_METHOD0(PauseReadingBodyFromNet, void());
    MOCK_METHOD0(ResumeReadingBodyFromNet, void());

   private:
    mojo::Receiver<network::mojom::URLLoader> receiver_;
  };

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SignedExchangeLoaderTest, Simple) {
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
  auto origin = url::Origin::Create(resource_request.url);
  resource_request.trusted_params = network::ResourceRequest::TrustedParams();
  resource_request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

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
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader =
      std::make_unique<SignedExchangeLoader>(
          resource_request, std::move(response), std::move(consumer_handle),
          std::move(client), std::move(endpoints),
          network::mojom::kURLLoadOptionNone,
          false /* should_redirect_to_fallback */, nullptr /* devtools_proxy */,
          nullptr /* reporter */, nullptr /* url_loader_factory */,
          SignedExchangeLoader::URLLoaderThrottlesGetter(), FrameTreeNodeId(),
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
          MockSignedExchangeHandler::kMockSxgPrefix + "Hello, world!",
          mojo::StringDataSource::AsyncWritingMode::
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
  EXPECT_CALL(mock_client_after_redirect, OnReceiveResponse(_, _, _));

  run_loop.Run();
  EXPECT_CALL(mock_client_after_redirect, OnComplete(_));

  signed_exchange_loader->ConnectToClient(std::move(client_after_redirect));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
