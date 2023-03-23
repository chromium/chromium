// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
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
        const absl::optional<GURL>& new_url) override {
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
                                                std::move(body), absl::nullopt);
  }

  void SimulateReceiveData(const std::string& data,
                           bool expected_successful = true) {
    ASSERT_TRUE(producer_handle_);
    uint32_t bytes_written = data.size();
    auto write_result = producer_handle_->WriteData(
        data.data(), &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
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

class TestURLLoaderClient : public network::mojom::URLLoaderClient,
                            public mojo::DataPipeDrainer::Client {
 public:
  TestURLLoaderClient() = default;
  ~TestURLLoaderClient() override = default;

  TestURLLoaderClient(const TestURLLoaderClient&) = delete;
  TestURLLoaderClient& operator=(const TestURLLoaderClient&) = delete;

  mojo::PendingReceiver<network::mojom::URLLoader>
  BindURLloaderAndGetReceiver() {
    return remote_.BindNewPipeAndPassReceiver();
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindURLLoaderClientAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void DisconnectMojoPipes() {
    remote_.reset();
    receiver_.reset();
  }

  std::string body_content() { return body_content_; }
  uint32_t total_bytes_read() { return total_bytes_read_; }
  bool body_finished() { return body_finished_; }
  int32_t total_transfer_size_diff() { return total_transfer_size_diff_; }

  absl::optional<network::URLLoaderCompletionStatus> completion_status() {
    return completion_status_;
  }

  const std::vector<
      std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>&
  received_redirects() {
    return received_redirects_;
  }

 private:
  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    NOTREACHED();
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override {
    EXPECT_EQ(cached_metadata, absl::nullopt);

    // Drains |body| into |body_content_|
    pipe_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    received_redirects_.emplace_back(redirect_info, std::move(head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    NOTREACHED();
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    total_transfer_size_diff_ += transfer_size_diff;
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    body_content_.append(
        std::string(static_cast<const char*>(data), num_bytes));
    total_bytes_read_ += num_bytes;
  }

  void OnDataComplete() override { body_finished_ = true; }

  mojo::Remote<network::mojom::URLLoader> remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  std::string body_content_;
  uint32_t total_bytes_read_{0};
  bool body_finished_{false};
  int32_t total_transfer_size_diff_{0};

  absl::optional<network::URLLoaderCompletionStatus> completion_status_;

  std::vector<std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>
      received_redirects_;
};

class PrefetchStreamingURLLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    test_url_loader_factory_ = std::make_unique<TestURLLoaderFactory>();
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return task_environment_.get();
  }

  TestURLLoaderFactory* test_url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<TestURLLoaderFactory> test_url_loader_factory_;
};

TEST_F(PrefetchStreamingURLLoaderTest, SuccessfulServedAfterCompletion) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates receiving the head and body for the prefetch.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->DisconnectMojoPipes();

  // Gets handler to serve prefetch from |streaming_loader|. After this
  // |streaming_loader| is self owned, so |weak_streaming_loader| should be used
  // after this point.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler request_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, SuccessfulServedBeforeCompletion) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent1 = "example";
  const std::string kBodyContent2 = " body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting the rest of the body
  // data. This should result in the data being streamed directly to the serving
  // URL loader.
  test_url_loader_factory()->SimulateReceiveHead(
      net::HTTP_OK, kBodyContent1.size() + kBodyContent2.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent1);

  // Gets handler to serve prefetch from |streaming_loader|. After this
  // |streaming_loader| is self owned, so |weak_streaming_loader| should be used
  // after this point.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler request_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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

  test_url_loader_factory()->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

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
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, SuccessfulNotServed) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates a successful prefetch that is not used.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, FailedInvalidHead) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_received_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                // This will cause the prefetch to be marked as not servable.
                return PrefetchStreamingURLLoaderStatus::kFailedInvalidHead;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](const network::URLLoaderCompletionStatus& completion_status) {
                NOTREACHED();
              }),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates a prefetch with a non-2XX response. This should be marked as not
  // servable.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_NOT_FOUND, 0);
  on_response_received_loop.Run();

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidHead, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, FailedNetError) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates a prefetch with a non-OK net error.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::ERR_FAILED);
  on_response_complete_loop.Run();

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetError, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, FailedNetErrorButServed) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting a net error. The error
  // should be passed to the serving URL loader.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);

  // Gets handler to serve prefetch from |streaming_loader|. After this
  // |streaming_loader| is self owned, so |weak_streaming_loader| should be used
  // after this point.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler request_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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

  test_url_loader_factory()->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::ERR_FAILED);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, ImmediateEligibleRedirect) {
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
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              },
              &on_receive_redirect_loop));

  ASSERT_TRUE(test_url_loader_factory()->test_url_loader());
  test_url_loader_factory()->test_url_loader()->SetOnFollowRedirectClosure(
      on_follow_redirect_loop.QuitClosure());

  // Simulate a redirect that should be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(kRedirectUrl,
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();
  on_follow_redirect_loop.Run();

  // Simulates receiving the prefetch after the redirect
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  // Simulates serving the redirect.
  EXPECT_FALSE(streaming_loader->IsReadyToServeFinalResponse());
  PrefetchStreamingURLLoader::RequestHandler redirect_handler =
      streaming_loader->ServingRedirectHandler();

  std::unique_ptr<TestURLLoaderClient> redirect_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(redirect_handler)
      .Run(serving_request,
           redirect_url_loader_client->BindURLloaderAndGetReceiver(),
           redirect_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Wait for the redirect to be sent to |redirect_url_loader_client|. Once the
  // redirect is served, |streaming_loader| will stop.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(redirect_url_loader_client->body_finished());
  EXPECT_EQ(redirect_url_loader_client->body_content(), "");
  EXPECT_EQ(redirect_url_loader_client->total_bytes_read(), 0U);

  EXPECT_FALSE(redirect_url_loader_client->completion_status());
  EXPECT_EQ(redirect_url_loader_client->received_redirects().size(), 1U);

  redirect_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(streaming_loader);

  // Simulates serving the final response.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler final_response_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, ImmediateIneligibleRedirect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce([](network::mojom::URLResponseHead* head) {
            NOTREACHED();
            return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
          }),
          base::BindOnce(
              [](const network::URLLoaderCompletionStatus& completion_status) {
                NOTREACHED();
              }),
          base::BindRepeating(
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect;
              },
              &on_receive_redirect_loop));

  // Simulate a redirect that should not be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, PausedEligibleRedirect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_follow_redirect_loop;
  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kPauseRedirectForEligibilityCheck;
              },
              &on_receive_redirect_loop));

  // Simulate a redirect that should be followed by the URL loader. The URL
  // loader needs to pause until the eligibility check is complete.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  ASSERT_TRUE(test_url_loader_factory()->test_url_loader());
  test_url_loader_factory()->test_url_loader()->SetOnFollowRedirectClosure(
      on_follow_redirect_loop.QuitClosure());

  streaming_loader->OnEligibilityCheckForRedirectComplete(/*is_eligible=*/true);
  on_follow_redirect_loop.Run();

  // Simulates receiving the prefetch after the redirect
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  // Simulates serving the redirect.
  EXPECT_FALSE(streaming_loader->IsReadyToServeFinalResponse());
  PrefetchStreamingURLLoader::RequestHandler redirect_handler =
      streaming_loader->ServingRedirectHandler();

  std::unique_ptr<TestURLLoaderClient> redirect_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

  network::ResourceRequest serving_request;
  serving_request.url = kTestUrl;
  serving_request.method = "GET";

  std::move(redirect_handler)
      .Run(serving_request,
           redirect_url_loader_client->BindURLloaderAndGetReceiver(),
           redirect_url_loader_client->BindURLLoaderClientAndGetRemote());

  // Wait for the redirect to be sent to |redirect_url_loader_client|. Once the
  // redirect is served, |streaming_loader| will stop.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(redirect_url_loader_client->body_finished());
  EXPECT_EQ(redirect_url_loader_client->body_content(), "");
  EXPECT_EQ(redirect_url_loader_client->total_bytes_read(), 0U);

  EXPECT_FALSE(redirect_url_loader_client->completion_status());
  EXPECT_EQ(redirect_url_loader_client->received_redirects().size(), 1U);

  redirect_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(streaming_loader);

  // Simulate serving the final response.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler final_response_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, PausedIneligibleRedirect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce([](network::mojom::URLResponseHead* head) {
            NOTREACHED();
            return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
          }),
          base::BindOnce(
              [](const network::URLLoaderCompletionStatus& completion_status) {
                NOTREACHED();
              }),
          base::BindRepeating(
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kPauseRedirectForEligibilityCheck;
              },
              &on_receive_redirect_loop));

  // Simulate a redirect that should not be followed by the URL loader.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  streaming_loader->OnEligibilityCheckForRedirectComplete(
      /*is_eligible=*/false);

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest,
       PausedEligibleRedirect_UrlLoaderDisconnect) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce([](network::mojom::URLResponseHead* head) {
            NOTREACHED();
            return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
          }),
          base::BindOnce(
              [](const network::URLLoaderCompletionStatus& completion_status) {
                NOTREACHED();
              }),
          base::BindRepeating(
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kPauseRedirectForEligibilityCheck;
              },
              &on_receive_redirect_loop));

  // Simulate a redirect that should be followed by the URL loader. The URL
  // loader needs to pause until the eligibility check is complete.
  test_url_loader_factory()->SimulateRedirect(GURL("https://redirect.com"),
                                              net::HTTP_PERMANENT_REDIRECT);
  on_receive_redirect_loop.Run();

  // Simulate the network URL loader stopping before the result of the
  // eligibility check is done.
  test_url_loader_factory()->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  streaming_loader->OnEligibilityCheckForRedirectComplete(/*is_eligible=*/true);

  // Since the network URL loader was disconnected, then redirect cannot be
  // followed and the prefetch should not be servable.
  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, Decoy) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates a successful prefetch that is not used. However, since the
  // prefetch is marked as a decoy, it cannot be served.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  // On a decoy, the body pipe is closed since the data should not be stored.
  test_url_loader_factory()->SimulateReceiveData(kBodyContent,
                                                 /*expected_successful=*/false);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, Timeout) {
  base::HistogramTester histogram_tester;
  const GURL kTestUrl = GURL("https://example.com");
  const std::string kBodyContent = "example body";

  std::unique_ptr<network::ResourceRequest> prefetch_request =
      std::make_unique<network::ResourceRequest>();
  prefetch_request->url = kTestUrl;
  prefetch_request->method = "GET";

  base::RunLoop on_response_complete_loop;

  // Create the |PrefetchStreamingURLLoader| that is being tested.
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::Seconds(1),
          base::BindOnce([](network::mojom::URLResponseHead* head) {
            NOTREACHED();
            return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
          }),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                EXPECT_EQ(completion_status.error_code, net::ERR_TIMED_OUT);
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  task_environment()->FastForwardBy(base::Seconds(1));
  on_response_complete_loop.Run();

  EXPECT_FALSE(streaming_loader->Servable(base::TimeDelta::Max()));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kFailedNetError, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, StopTimeoutTimerAfterBeingServed) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::Seconds(1),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                EXPECT_EQ(completion_status.error_code, net::OK);
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates receiving the head of the prefetch response.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  // Simulate serving the prefetch. This should stop the timeout timer.
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler request_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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

  EXPECT_TRUE(weak_streaming_loader->Servable(base::TimeDelta::Max()));

  test_url_loader_factory()->DisconnectMojoPipes();

  // Wait for the data to be drained from the body pipe.
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);

  serving_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, StaleResponse) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates a successful prefetch that is not used.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  task_environment()->FastForwardBy(base::Seconds(2));

  // The staleness of the streaming URL loader response is measured from when
  // the response is complete, not when the head is received.
  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta()));

  test_url_loader_factory()->SimulateReceiveData(kBodyContent);
  test_url_loader_factory()->SimulateResponseComplete(net::OK);
  on_response_complete_loop.Run();

  task_environment()->FastForwardBy(base::Seconds(4));

  // The response should not be servable if its been too long since it has
  // completed.
  EXPECT_FALSE(streaming_loader->Servable(base::Seconds(3)));
  EXPECT_FALSE(streaming_loader->Servable(base::Seconds(4)));
  EXPECT_TRUE(streaming_loader->Servable(base::Seconds(5)));

  streaming_loader.reset();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed, 1);
}

TEST_F(PrefetchStreamingURLLoaderTest, TransferSizeUpdated) {
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
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          test_url_loader_factory(), std::move(prefetch_request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              }));

  // Simulates receiving the head for the prefetch, receiving part of the body
  // data, start to serve the prefetch, and then getting the rest of the body
  // data. This should result in the data being streamed directly to the serving
  // URL loader.
  test_url_loader_factory()->SimulateReceiveHead(net::HTTP_OK,
                                                 kBodyContent.size());
  on_response_received_loop.Run();

  EXPECT_TRUE(streaming_loader->Servable(base::TimeDelta::Max()));

  // Simulates updating the transfer size. This event will be queued in the
  // streaming URL loader and sent to the serving URL loader once bound.
  test_url_loader_factory()->SimulateTransferSizeUpdated(100);

  // Gets handler to serve prefetch from |streaming_loader|. After this
  // |streaming_loader| is self owned, so |weak_streaming_loader| should be used
  // after this point.
  EXPECT_TRUE(streaming_loader->IsReadyToServeFinalResponse());
  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  PrefetchStreamingURLLoader::RequestHandler request_handler =
      weak_streaming_loader->ServingFinalResponseHandler(
          std::move(streaming_loader));

  // Set up URLLoaderClient to "serve" the prefetch.
  std::unique_ptr<TestURLLoaderClient> serving_url_loader_client =
      std::make_unique<TestURLLoaderClient>();

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

  test_url_loader_factory()->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(serving_url_loader_client->body_finished());
  EXPECT_EQ(serving_url_loader_client->body_content(), kBodyContent);
  EXPECT_EQ(serving_url_loader_client->total_bytes_read(), kBodyContent.size());

  EXPECT_TRUE(serving_url_loader_client->completion_status());
  EXPECT_EQ(serving_url_loader_client->completion_status()->error_code,
            net::OK);
  EXPECT_EQ(serving_url_loader_client->received_redirects().size(), 0U);

  serving_url_loader_client->DisconnectMojoPipes();
  task_environment()->RunUntilIdle();

  // Once the streaming URL loader serves is finished (all prefetched data
  // received and served) and all mojo pipes are disconnected, it should delete
  // itself.
  EXPECT_FALSE(weak_streaming_loader);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
      PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion, 1);
}

}  // namespace
}  // namespace content
