// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class TestURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  class TestURLLoader : public network::mojom::URLLoader {
   public:
    explicit TestURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver)
        : receiver_(this, std::move(receiver)) {}
    ~TestURLLoader() override = default;

    void SetOnFollowRedirectClosure(base::OnceClosure closure) {
      on_follow_redirect_closure_ = std::move(closure);
    }

    // network::mojom::URLLoader
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const std::optional<GURL>& new_url) override {
      EXPECT_EQ(removed_headers.size(), 0U);
      EXPECT_TRUE(modified_headers.IsEmpty());
      EXPECT_TRUE(modified_cors_exempt_headers.IsEmpty());
      EXPECT_FALSE(new_url);

      ASSERT_TRUE(on_follow_redirect_closure_);
      std::move(on_follow_redirect_closure_).Run();
    }

    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override {}
    void PauseReadingBodyFromNet() override {}
    void ResumeReadingBodyFromNet() override {}

   private:
    base::OnceClosure on_follow_redirect_closure_;
    mojo::Receiver<network::mojom::URLLoader> receiver_;
  };

  TestURLLoaderFactory() = default;
  ~TestURLLoaderFactory() override = default;

  TestURLLoaderFactory(const TestURLLoaderFactory&) = delete;
  TestURLLoaderFactory& operator=(const TestURLLoaderFactory&) = delete;

  void SimulateReceiveHead(net::HttpStatusCode http_status,
                           uint32_t expected_total_body_size) {
    ASSERT_TRUE(streaming_client_remote_);

    auto head = network::CreateURLResponseHead(http_status);

    mojo::ScopedDataPipeConsumerHandle body;
    if (expected_total_body_size > 0) {
      EXPECT_EQ(mojo::CreateDataPipe(expected_total_body_size, producer_handle_,
                                     body),
                MOJO_RESULT_OK);
    }

    streaming_client_remote_->OnReceiveResponse(std::move(head),
                                                std::move(body), std::nullopt);
  }

  void SimulateReceiveData(const std::string& data,
                           bool expected_successful = true) {
    ASSERT_TRUE(producer_handle_);
    MojoResult write_result =
        producer_handle_->WriteAllData(base::as_byte_span(data));

    if (expected_successful) {
      EXPECT_EQ(write_result, MOJO_RESULT_OK);
    } else {
      EXPECT_NE(write_result, MOJO_RESULT_OK);
    }
  }

  void SimulateResponseComplete(net::Error net_error) {
    producer_handle_.reset();

    network::URLLoaderCompletionStatus completion_status(net_error);
    streaming_client_remote_->OnComplete(completion_status);
  }

  void SimulateRedirect(const GURL& redirect_url,
                        net::HttpStatusCode http_status) {
    ASSERT_TRUE(streaming_client_remote_);

    net::RedirectInfo redirect_info;
    redirect_info.new_url = redirect_url;

    auto head = network::CreateURLResponseHead(http_status);

    streaming_client_remote_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  void SimulateTransferSizeUpdated(int32_t transfer_size_diff) {
    ASSERT_TRUE(streaming_client_remote_);
    streaming_client_remote_->OnTransferSizeUpdated(transfer_size_diff);
  }

  bool IsURLLoaderClientConnected() {
    return streaming_client_remote_.is_connected();
  }

  void DisconnectMojoPipes() {
    EXPECT_TRUE(streaming_client_remote_);
    streaming_client_remote_.reset();
  }

  TestURLLoader* test_url_loader() { return test_url_loader_.get(); }

 private:
  // network::mojom::URLLoaderFactory
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    ASSERT_FALSE(streaming_client_remote_);
    ASSERT_FALSE(test_url_loader_);
    EXPECT_EQ(request_id, 0);
    EXPECT_EQ(options,
              network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
                  network::mojom::kURLLoadOptionSniffMimeType |
                  network::mojom::kURLLoadOptionSendSSLInfoForCertificateError);

    streaming_client_remote_.Bind(std::move(client));
    test_url_loader_ = std::make_unique<TestURLLoader>(std::move(receiver));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    receivers_.Add(this, std::move(receiver));
  }

  mojo::Remote<network::mojom::URLLoaderClient> streaming_client_remote_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<TestURLLoader> test_url_loader_;
};

class PrefetchStreamingURLLoaderTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<bool, PrefetchReusableForTests>> {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    test_url_loader_factory_ = std::make_unique<TestURLLoaderFactory>();

    switch (std::get<1>(GetParam())) {
      case PrefetchReusableForTests::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(features::kPrefetchReusable);
        break;
    }
  }

  void TearDown() override { scoped_feature_list_.Reset(); }

  base::test::TaskEnvironment* task_environment() {
    return task_environment_.get();
  }

  TestURLLoaderFactory* test_url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<TestURLLoaderFactory> test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The first parameter should determine if the test should call
// SetOnReceivedHeadCallback and check that callback is later called.
INSTANTIATE_TEST_SUITE_P(
    All,
    PrefetchStreamingURLLoaderTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(PrefetchReusableValuesForTests())));

TEST_P(PrefetchStreamingURLLoaderTest, SuccessfulServedAfterCompletion) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates receiving the head and body for the prefetch.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  // Gets handler to serve prefetch from |reseponse_reader|. After this
  // |response_reader| is self owned, so |weak_response_reader| should be used
  // after this point.
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler request_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(request_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Wait for the data to be drained from the body pipe.
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, SuccessfulServedBeforeCompletion) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent1 = "example";
  const std::string kBodyContent2 = " body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting the rest of the body
  // data. This should result in the data being streamed directly to the serving
  // URL loader.
  test_url_loader_factory()->SimulateReceiveHead(
      net::HTTP_OK, kBodyContent1.size() + kBodyContent2.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent1);

  // Gets handler to serve prefetch from |reseponse_reader|. After this
  // |response_reader| is self owned, so |weak_response_reader| should be used
  // after this point.
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler request_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(request_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());
  task_environment()->RunUntilIdle();

  // The serving URL loader should immediately get the data that has been
  // received so far.
  EXPECT_FALSE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent1);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(),
            kBodyContent1.size());

  EXPECT_FALSE(serving_url_loader_client->completion_status());

  // The rest of the data is received. This data should be directly streamed to
  // the serving URL loader.
  test_url_loader_factory()->SimulateReceiveData(kBodyContent2);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(),
            kBodyContent1 + kBodyContent2);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(),
            kBodyContent1.size() + kBodyContent2.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, SuccessfulNotServed) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates a successful prefetch that is not used.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, FailedInvalidHead) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            // This will cause the prefetch to be marked as not servable.
            return std::make_optional(
                PrefetchErrorOnResponseReceived::kFailedInvalidHead);
          },
          &on_response_received_loop),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED_IN_MIGRATION();
          }),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates a prefetch with a non-2XX response. This should be marked as not
  // servable.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_NOT_FOUND, 0);
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  response_reader.reset();

  // Streaming loader deletes itself asynchronously once prefetching URL loader
  // is disconnected.
  test_url_loader_factory()->DisconnectMojoPipes();
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidHead, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, FailedNetError_HeadReceived) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates a prefetch with a non-OK net error.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::ERR_FAILED);
  on_response_complete_loop.Run();

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetError, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, FailedNetError_HeadNotReveived) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulate getting a non-OK net error.
  test_url_loader_factory()->SimulateResponseComplete(net::ERR_FAILED);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetError, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, FailedNetErrorButServed) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting a net error. The error
  // should be passed to the serving URL loader.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);

  // Gets handler to serve prefetch from |reseponse_reader|. After this
  // |response_reader| is self owned, so |weak_response_reader| should be used
  // after this point.
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler request_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(request_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());
  task_environment()->RunUntilIdle();

  // The serving URL loader should immediately get the data that has been
  // received so far.
  EXPECT_FALSE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_FALSE(serving_url_loader_client->completion_status());

  // Once the net error is received, the serving URL loader should be notified.
  test_url_loader_factory()->SimulateResponseComplete(net::ERR_FAILED);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::ERR_FAILED);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, EligibleRedirect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const GURL kRedirectUrl = GURL("https://redirect.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_follow_redirect_loop;
  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto redirect_response_reader =
      base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      redirect_response_reader->GetWeakPtr());

  ASSERT_TRUE(test_url_loader_factory()->test_url_loader());
  test_url_loader_factory()->test_url_loader()->SetOnFollowRedirectClosure(
      on_follow_redirect_loop.QuitClosure());

  // Simulate a redirect that should be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(kRedirectUrl,
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  ASSERT_TRUE(streaming_loader);
  streaming_loader->HandleRedirect(PrefetchRedirectStatus::kFollow,
                                   redirect_info, std::move(redirect_head));
  on_follow_redirect_loop.Run();

  // Switch to a new ResponseReader.
  auto final_response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  ASSERT_TRUE(streaming_loader);
  streaming_loader->SetResponseReader(final_response_reader->GetWeakPtr());

  // Simulates receiving the prefetch after the redirect
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_TRUE(final_response_reader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_TRUE(final_response_reader->Servable(base::TimeDelta::Max()));

  // Simulates serving the redirect.
  base::WeakPtr<PrefetchResponseReader> weak_redirect_response_reader =
      redirect_response_reader->GetWeakPtr();
  PrefetchRequestHandler redirect_handler =
      weak_redirect_response_reader->CreateRequestHandler();
  redirect_response_reader.reset();

  std::unique_ptr<PrefetchTestURLLoaderClient> redirect_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  ASSERT_TRUE(redirect_handler);
  std::move(redirect_handler)
      .Run(serving_request,
           redirect_url_loader_client->BindURLloaderAndGetReceiver(),
           redirect_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Wait for the redirect to be sent to |redirect_url_loader_client|.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(redirect_url_loader_client->body_finished());
  EXPECT_EQ(redirect_url_loader_client->body_content(), "");
  EXPECT_EQ(redirect_url_loader_client->total_bytes_read(), 0U);

  EXPECT_FALSE(redirect_url_loader_client->completion_status());
  EXPECT_EQ(redirect_url_loader_client->received_redirects().size(), 1U);

  redirect_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_redirect_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (the redirect is
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself while the streaming loader is still alive.
  EXPECT_FALSE(weak_redirect_response_reader);

  // Simulates serving the final response.
  base::WeakPtr<PrefetchResponseReader> weak_final_response_reader =
      final_response_reader->GetWeakPtr();
  PrefetchRequestHandler final_response_handler =
      weak_final_response_reader->CreateRequestHandler();
  final_response_reader.reset();

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  std::move(final_response_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Wait for the data to be drained from the body pipe.
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_final_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_final_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, IneligibleRedirect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_head_received_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED_IN_MIGRATION();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulate a redirect that should not be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  ASSERT_TRUE(streaming_loader);
  streaming_loader->HandleRedirect(PrefetchRedirectStatus::kFail, redirect_info,
                                   std::move(redirect_head));

  // Streaming loader deletes itself asynchronously on redirect failure.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, RedirectSwitchInNetworkContext) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED_IN_MIGRATION();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      // When a redirect causes a change in network context, the
      // on_receive_head_callback_ is not called, and is passed to the
      // follow up PrefetchStreamingURLLoader that will follow the redirect
      // in the other network context.
      std::get<0>(GetParam())
          ? base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); })
          : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulate a redirect that should not be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  // Simulate an eligible redirect that requires a change in the network
  // context. When this happens the streaming_loader will stop the fetch, and a
  // new streaming URL loader would start to fetch the redirect URL.
  ASSERT_TRUE(streaming_loader);
  streaming_loader->HandleRedirect(
      PrefetchRedirectStatus::kSwitchNetworkContext, redirect_info,
      std::move(redirect_head));

  // Streaming loader deletes itself asynchronously on a switching redirect.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_FALSE(test_url_loader_factory()->IsURLLoaderClientConnected());

  // The response_reader is marked as not servable, but it can serve the
  // redirect. The follow up streaming URL loader would then continue serving
  // the prefetch.
  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler redirect_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(redirect_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  task_environment()->RunUntilIdle();

  // The response_reader should only serve a redirect.
  EXPECT_FALSE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), "");
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), 0U);
  EXPECT_FALSE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 1U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::
          kServedSwitchInNetworkContextForRedirect,
      1);
}

TEST_P(PrefetchStreamingURLLoaderTest,
       PausedEligibleRedirect_UrlLoaderDisconnect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_deletion_scheduled_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED_IN_MIGRATION();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());
  streaming_loader->SetOnDeletionScheduledForTests(
      on_deletion_scheduled_loop.QuitClosure());

  // Simulate a redirect that should be followed by the URL loader. The URL
  // loader needs to pause until the eligibility check is complete.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  // Simulate the result of the eligibility check is done after the network URL
  // loader stops and before streaming loader is deleted.
  test_url_loader_factory()->DisconnectMojoPipes();
  on_deletion_scheduled_loop.Run();
  ASSERT_TRUE(streaming_loader);
  streaming_loader->HandleRedirect(PrefetchRedirectStatus::kFollow,
                                   redirect_info, std::move(redirect_head));
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }
  task_environment()->RunUntilIdle();

  // Streaming loader deletes itself asynchronously once prefetching URL loader
  // is disconnected.
  EXPECT_FALSE(streaming_loader);

  // Since the network URL loader was disconnected, then redirect cannot be
  // followed and the prefetch should not be servable.
  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, Decoy) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_head_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::make_optional(
                PrefetchErrorOnResponseReceived::kPrefetchWasDecoy);
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  // Simulates a successful prefetch that is not used. However, since the
  // prefetch is marked as a decoy, it cannot be served.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  // On a decoy, the body pipe is closed since the data should not be stored.
  test_url_loader_factory()->SimulateReceiveData(kBodyContent,
                                                 /*expected_successful=*/false);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  response_reader.reset();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, Timeout) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_complete_loop;
  base::RunLoop on_head_received_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::Seconds(1),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            EXPECT_EQ(completion_status.error_code, net::ERR_TIMED_OUT);
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      std::get<0>(GetParam()) ? on_head_received_loop.QuitClosure()
                              : base::OnceClosure(),
      response_reader->GetWeakPtr());

  task_environment()->FastForwardBy(base::Seconds(1));
  on_response_complete_loop.Run();
  if (std::get<0>(GetParam())) {
    on_head_received_loop.Run();
  }

  // Disconnected due to timeout.
  EXPECT_FALSE(test_url_loader_factory()->IsURLLoaderClientConnected());

  // Streaming loader deletes itself asynchronously once prefetching URL loader
  // is disconnected.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_FALSE(response_reader->Servable(base::TimeDelta::Max()));

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetError, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, StopTimeoutTimerAfterBeingServed) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::Seconds(1),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            EXPECT_EQ(completion_status.error_code, net::OK);
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      base::OnceClosure(), response_reader->GetWeakPtr());

  // Simulates receiving the head of the prefetch response.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  // Simulate serving the prefetch. This should stop the timeout timer.
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler request_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(request_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Since the prefetch has been served, the timeout trigger should not be
  // triggered.
  task_environment()->FastForwardBy(base::Seconds(10));

  // Simulate receiving the body of the response.
  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  ASSERT_TRUE(weak_response_reader);
  EXPECT_TRUE(weak_response_reader->Servable(base::TimeDelta::Max()));

  // Wait for the data to be drained from the body pipe.
  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);

  ASSERT_TRUE(weak_response_reader);
  EXPECT_TRUE(weak_response_reader->Servable(base::TimeDelta::Max()));

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, StaleResponse) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      base::OnceClosure(), response_reader->GetWeakPtr());

  // Simulates a successful prefetch that is not used.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  task_environment()->FastForwardBy(base::Seconds(2));

  // The staleness of the streaming URL loader response is measured from when
  // the response is complete, not when the head is received.
  EXPECT_TRUE(response_reader->Servable(base::TimeDelta()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  task_environment()->FastForwardBy(base::Seconds(4));

  // The response should not be servable if its been too long since it has
  // completed.
  EXPECT_FALSE(response_reader->Servable(base::Seconds(3)));
  EXPECT_FALSE(response_reader->Servable(base::Seconds(4)));
  EXPECT_TRUE(response_reader->Servable(base::Seconds(5)));

  response_reader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, TransferSizeUpdated) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  auto response_reader = base::MakeRefCounted<PrefetchResponseReader>();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      test_url_loader_factory(), *prefetch_request,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      base::OnceClosure(), response_reader->GetWeakPtr());

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting the rest of the body
  // data. This should result in the data being streamed directly to the serving
  // URL loader.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(response_reader->Servable(base::TimeDelta::Max()));

  // Simulates updating the transfer size. This event will be queued in the
  // streaming URL loader and sent to the serving URL loader once bound.
  test_url_loader_factory()->SimulateTransferSizeUpdated(100);

  // Gets handler to serve prefetch from |reseponse_reader|. After this
  // |response_reader| is self owned, so |weak_response_reader| should be used
  // after this point.
  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      response_reader->GetWeakPtr();
  PrefetchRequestHandler request_handler =
      weak_response_reader->CreateRequestHandler();
  response_reader.reset();

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<PrefetchTestURLLoaderClient> serving_url_loader_client =
      std::make_unique<PrefetchTestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(request_handler)
      .Run(serving_request,
           serving_url_loader_client->BindURLloaderAndGetReceiver(),
           serving_url_loader_client->BindURLLoaderClientAndGetRemote());
  task_environment()->RunUntilIdle();

  // The serving URL loader should immediately get the queued events.
  EXPECT_EQ(serving_url_loader_client->total_transfer_size_diff(), 100);

  EXPECT_FALSE(serving_url_loader_client->completion_status());

  // Simulates another transfer size update. Since the serving URL loader is
  // bound, it should be immediately sent to it.
  test_url_loader_factory()->SimulateTransferSizeUpdated(200);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(serving_url_loader_client->total_transfer_size_diff(), 300);

  // The rest of the data is received. This data should be directly streamed to
  // the serving URL loader.
  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  // Streaming loader deletes itself asynchronously on prefetch completion.
  EXPECT_TRUE(streaming_loader);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(streaming_loader);

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();

  EXPECT_TRUE(weak_response_reader);
  task_environment()->RunUntilIdle();
  // Once the `PrefetchResponseReader` serves is finished (all prefetched data
  // served) and the serving mojo pipe is disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_response_reader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

TEST_P(PrefetchStreamingURLLoaderTest, DoesNotTakeDevToolsObserver) {
  // These Mojo interfaces aren't actually hooked up to anything, but they
  // suffice to trigger the same serialization code as in production.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> url_loader_factory;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_remote(
      url_loader_factory.InitWithNewPipeAndPassRemote());
  mojo::PendingReceiver<network::mojom::DevToolsObserver> observer;

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com");
  request.trusted_params.emplace().devtools_observer =
      observer.InitWithNewPipeAndPassRemote();

  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      url_loader_factory_remote.get(), request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead*) {
        return std::make_optional(
            PrefetchErrorOnResponseReceived::kFailedInvalidHead);
      }),
      base::DoNothing(), base::DoNothing(), base::DoNothing(), nullptr);
  EXPECT_TRUE(request.trusted_params->devtools_observer.is_valid());
}

}  // namespace
}  // namespace content
