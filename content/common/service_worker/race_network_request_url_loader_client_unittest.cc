// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_url_loader_client.h"

#include "base/containers/span.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
MojoResult CreateDataPipe(mojo::ScopedDataPipeProducerHandle& producer_handle,
                          mojo::ScopedDataPipeConsumerHandle& consumer_handle) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}

std::unique_ptr<network::ResourceRequest> CreateRequest() {
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com/");
  request->method = "GET";
  request->mode = network::mojom::RequestMode::kNavigate;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->redirect_mode = network::mojom::RedirectMode::kManual;
  request->destination = network::mojom::RequestDestination::kDocument;
  return request;
}

enum class State {
  kWaiting,
  kChunkReceived,
  kAllChunkReceived,
};

}  // namespace

using OnCommitResponseCallback = base::OnceCallback<void(
    const network::mojom::URLResponseHeadPtr& response_head,
    mojo::ScopedDataPipeConsumerHandle body)>;
using OnCompletedCallback =
    base::OnceCallback<void(int error_code, const char* reason)>;

class MockServiceWorkerResourceLoader : public ServiceWorkerResourceLoader {
 public:
  bool IsMainResourceLoader() override { return true; }
  void CommitResponseHeaders(
      const network::mojom::URLResponseHeadPtr& response_head) override {}
  void CommitResponseBody(
      const network::mojom::URLResponseHeadPtr& response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    std::move(on_commit_response_).Run(response_head, std::move(response_body));
  }
  void CommitEmptyResponseAndComplete() override {}
  void CommitCompleted(int error_code, const char* reason) override {
    std::move(on_commit_completed_).Run(error_code, reason);
  }
  void HandleRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHeadPtr& response_head) override {}

  base::WeakPtr<MockServiceWorkerResourceLoader> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetOnCommitResponseCallback(OnCommitResponseCallback callback) {
    on_commit_response_ = std::move(callback);
  }

  void SetOnCompletedCallback(OnCompletedCallback callback) {
    on_commit_completed_ = std::move(callback);
  }

 private:
  OnCommitResponseCallback on_commit_response_;
  OnCompletedCallback on_commit_completed_;
  base::WeakPtrFactory<MockServiceWorkerResourceLoader> weak_factory_{this};
};

// This class acts as end points of data pipe consumers, for both
// RaceNetworkRequest and the fetch handler.
// Watches body data as a data pipe consumer handle from the test response. If
// the data pipe is readable, read the chunk of data which is currently in the
// data pipe, and store it into |chunk_| to confirm if the actual received data
// is expected one.
// This class also manages the data transfer status in |state_|, which indicates
// the current progress during the data transfer process.
class ResponseBodyDataPipeReader {
 public:
  void WatchResponseBody(
      const network::mojom::URLResponseHeadPtr& response_head,
      mojo::ScopedDataPipeConsumerHandle body) {
    body_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
        base::SequencedTaskRunner::GetCurrentDefault());
    body_ = std::move(body);
    body_watcher_->Watch(
        body_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&ResponseBodyDataPipeReader::OnReadlable,
                            base::Unretained(this)));
    body_watcher_->ArmOrNotify();
  }

  void OnReadlable(MojoResult) {
    // If the current |state_| is not the status waiting for new chunk, recall
    // the function until |state_| has changed.
    if (state_ != State::kWaiting) {
      body_watcher_->ArmOrNotify();
      return;
    }
    base::span<const uint8_t> buffer;
    MojoResult result =
        body_->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);
    switch (result) {
      case MOJO_RESULT_OK:
        chunk_ = std::string(base::as_string_view(buffer));
        state_ = State::kChunkReceived;
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // Signal for data transfer completion.
        state_ = State::kAllChunkReceived;
        body_watcher_->Cancel();
        break;
      default:
        break;
    }
  }

  std::string ConsumeChunk() {
    const std::string chunk = chunk_;
    EXPECT_EQ(body_->EndReadData(chunk_.size()), MOJO_RESULT_OK);
    chunk_ = "";

    return chunk;
  }

  State state() { return state_; }

  void RunUntilStateChange(bool resume_state) {
    if (resume_state) {
      state_ = State::kWaiting;
      body_watcher_->ArmOrNotify();
    }
    while (true) {
      if (state() != State::kWaiting) {
        break;
      }
      base::RunLoop().RunUntilIdle();
    }
  }

  void AbortBodyConsumerHandle() { body_.reset(); }

  bool IsDisconnected() {
    return body_->EndReadData(chunk_.size()) == MOJO_RESULT_FAILED_PRECONDITION;
  }

 private:
  std::unique_ptr<mojo::SimpleWatcher> body_watcher_;
  mojo::ScopedDataPipeConsumerHandle body_;
  std::string chunk_;
  State state_ = State::kWaiting;
};

class URLLoaderClientForFetchHandler : public network::mojom::URLLoaderClient,
                                       public ResponseBodyDataPipeReader {
 public:
  void Bind(mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
    receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
  }

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    WatchResponseBody(head, std::move(body));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {}

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
};

class ServiceWorkerRaceNetworkRequestURLLoaderClientTest
    : public testing::Test,
      public ResponseBodyDataPipeReader {
 public:
  ServiceWorkerRaceNetworkRequestURLLoaderClientTest()
      : owner_(std::make_unique<MockServiceWorkerResourceLoader>()),
        client_for_fetch_handler_(
            std::make_unique<URLLoaderClientForFetchHandler>()) {}

  // Write test data into the data pipe as a mock response. And this method
  // calls |client_|'s OnReceiveResponse(), which will trigger the relay of data
  // chunks in ServiceWorkerRaceNetworkRequestURLLoaderClient.
  void WriteData(const std::string& expected_body) {
    size_t actually_written_bytes = 0;
    MojoResult result =
        producer_->WriteData(base::as_byte_span(expected_body),
                             MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    ASSERT_EQ(result, MOJO_RESULT_OK);
    network::mojom::URLResponseHeadPtr head(
        network::CreateURLResponseHead(net::HTTP_OK));
    client_->OnReceiveResponse(std::move(head), std::move(consumer_),
                               std::nullopt);
    producer_.reset();
  }

  // Tells |clients_| to the completion of the response.
  void CompleteResponse(const net::Error& error_code) {
    const network::URLLoaderCompletionStatus status(error_code);
    client_->OnComplete(status);
  }

  MockServiceWorkerResourceLoader* owner() { return owner_.get(); }

  void CloseConnection() { producer_.reset(); }

  URLLoaderClientForFetchHandler* client_for_fetch_handler() {
    return client_for_fetch_handler_.get();
  }

 protected:
  void SetUp() override {
    ASSERT_EQ(CreateDataPipe(producer_, consumer_), MOJO_RESULT_OK);
  }

  void SetUpURLLoaderClient(uint32_t data_pipe_capacity_num_bytes) {
    RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
        data_pipe_capacity_num_bytes);
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client;
    client_for_fetch_handler_->Bind(&forwarding_client);
    client_ = std::make_unique<ServiceWorkerRaceNetworkRequestURLLoaderClient>(
        *CreateRequest(), owner_->GetWeakPtr(), std::move(forwarding_client));
    EXPECT_EQ(
        client_->state(),
        ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kWaitForBody);
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_to_pass;
    client_->Bind(&client_to_pass);
  }

  void SetOnCommitResponseCallback(OnCommitResponseCallback callback) {
    owner_->SetOnCommitResponseCallback(std::move(callback));
  }

  void SetOnCompletedCallback(OnCompletedCallback callback) {
    owner_->SetOnCompletedCallback(std::move(callback));
  }

  ServiceWorkerRaceNetworkRequestURLLoaderClient::State client_state() {
    return client_->state();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::ScopedDataPipeProducerHandle producer_;
  mojo::ScopedDataPipeConsumerHandle consumer_;
  std::unique_ptr<MockServiceWorkerResourceLoader> owner_;
  std::unique_ptr<ServiceWorkerRaceNetworkRequestURLLoaderClient> client_;
  std::unique_ptr<URLLoaderClientForFetchHandler> client_for_fetch_handler_;
};

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest, Basic) {
  SetUpURLLoaderClient(network::features::GetDataPipeDefaultAllocationSize());

  const std::string kExpectedBody = "abc";
  WriteData(kExpectedBody);

  base::RunLoop run_loop;
  SetOnCommitResponseCallback(base::BindOnce(
      [](std::string expected_body,
         const network::mojom::URLResponseHeadPtr& response_head,
         mojo::ScopedDataPipeConsumerHandle body) {
        base::span<const uint8_t> buffer;
        MojoResult result =
            body->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);
        ASSERT_EQ(result, MOJO_RESULT_OK);
        EXPECT_EQ(base::as_string_view(buffer), expected_body);
        result = body->EndReadData(buffer.size());
        ASSERT_EQ(result, MOJO_RESULT_OK);
      },
      kExpectedBody));
  SetOnCompletedCallback(base::BindOnce(
      [](base::OnceClosure done,
         scoped_refptr<base::SequencedTaskRunner> task_runner, int error_code,
         const char* reason) {
        EXPECT_EQ(error_code, net::OK);
        task_runner->PostTask(FROM_HERE, std::move(done));
      },
      run_loop.QuitClosure(), base::SequencedTaskRunner::GetCurrentDefault()));
  CompleteResponse(net::OK);
  run_loop.Run();

  // Check the response for fetch handler
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(client_for_fetch_handler()->ConsumeChunk(), kExpectedBody);
}

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest,
       LargeDataOverBufferSize) {
  const uint32_t data_pipe_capacity_num_bytes = 8;
  SetUpURLLoaderClient(data_pipe_capacity_num_bytes);

  // Expected input size should be larger than the data pipe size.
  const std::string kExpectedBody = "abcdefghijklmnop";
  ASSERT_GT(kExpectedBody.size(), data_pipe_capacity_num_bytes);
  WriteData(kExpectedBody);

  // Set the callback for OnCommitResponse. This callback start watching the
  // response body data pipe.
  SetOnCommitResponseCallback(base::BindOnce(
      &ServiceWorkerRaceNetworkRequestURLLoaderClientTest::WatchResponseBody,
      base::Unretained(this)));

  // Set the callback for the commit completion.
  SetOnCompletedCallback(base::BindOnce([](int error_code, const char* reason) {
    EXPECT_EQ(error_code, net::OK);
  }));
  CompleteResponse(net::OK);

  // Waiting for the first data chunk is received. The first chunk is the
  // fragment of the expected input, which is splitted per the data pipe size.
  const std::string first_chunk =
      kExpectedBody.substr(0, data_pipe_capacity_num_bytes);
  RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(state(), State::kChunkReceived);
  EXPECT_EQ(ConsumeChunk(), first_chunk);

  // Check the client for fetch handler side.
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(client_for_fetch_handler()->ConsumeChunk(), first_chunk);

  // Waiting for the second data chunk is received. The second chunk is the rest
  // of the expected input.
  const std::string second_chunk = kExpectedBody.substr(
      data_pipe_capacity_num_bytes, data_pipe_capacity_num_bytes);
  RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(state(), State::kChunkReceived);
  EXPECT_EQ(ConsumeChunk(), second_chunk);

  // Check the client for fetch handler side.
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(client_for_fetch_handler()->ConsumeChunk(), second_chunk);

  // Waiting for the data pipe to receive the completion signal.
  RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(state(), State::kAllChunkReceived);
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(client_for_fetch_handler()->state(), State::kAllChunkReceived);
}

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest,
       LargeDataOverBufferSize_SlowConsuming) {
  const uint32_t data_pipe_capacity_num_bytes = 4;
  SetUpURLLoaderClient(data_pipe_capacity_num_bytes);

  // Expected input size should be larger than the data pipe size.
  const std::string kExpectedBody = "abcdefghijklmnop";
  ASSERT_GT(kExpectedBody.size(), data_pipe_capacity_num_bytes);
  WriteData(kExpectedBody);

  // Set the callback for OnCommitResponse. This callback start watching the
  // response body data pipe.
  SetOnCommitResponseCallback(base::BindOnce(
      &ServiceWorkerRaceNetworkRequestURLLoaderClientTest::WatchResponseBody,
      base::Unretained(this)));

  // Set the callback for the commit completion.
  SetOnCompletedCallback(base::BindOnce([](int error_code, const char* reason) {
    EXPECT_EQ(error_code, net::OK);
  }));
  CompleteResponse(net::OK);

  // Waiting for the first data chunk is received. The first chunk is the
  // fragment of the expected input, which is splitted per the data pipe size.
  const std::string first_chunk =
      kExpectedBody.substr(0, data_pipe_capacity_num_bytes);
  RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(state(), State::kChunkReceived);
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(client_for_fetch_handler()->state(), State::kChunkReceived);

  // Consume the chunk in the data pipe for the fetch handler first to let
  // ServiceWorkerRaceNetworkRequestURLLoaderClient retry writing to data pipes
  // by getting |MOJO_RESULT_SHOULD_WAIT|.
  EXPECT_EQ(client_for_fetch_handler()->ConsumeChunk(), first_chunk);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ConsumeChunk(), first_chunk);

  // Consume the second chunk.
  const std::string second_chunk = kExpectedBody.substr(
      data_pipe_capacity_num_bytes, data_pipe_capacity_num_bytes);
  RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(state(), State::kChunkReceived);
  EXPECT_EQ(ConsumeChunk(), second_chunk);
  base::RunLoop().RunUntilIdle();
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/true);
  EXPECT_EQ(client_for_fetch_handler()->ConsumeChunk(), second_chunk);
}

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest,
       DataPipeDisconnected) {
  const uint32_t data_pipe_capacity_num_bytes = 8;
  SetUpURLLoaderClient(data_pipe_capacity_num_bytes);

  // Expected input size should be larger than the data pipe size.
  const std::string kExpectedBody = "abcdefghijklmnopqrstu";
  ASSERT_GT(kExpectedBody.size(), data_pipe_capacity_num_bytes);
  WriteData(kExpectedBody);

  // Set the callback for OnCommitResponse. This callback start watching the
  // response body data pipe.
  SetOnCommitResponseCallback(base::BindOnce(
      &ServiceWorkerRaceNetworkRequestURLLoaderClientTest::WatchResponseBody,
      base::Unretained(this)));

  // Set the callback for the commit completion.
  SetOnCompletedCallback(base::BindOnce([](int error_code, const char* reason) {
    EXPECT_EQ(error_code, net::OK);
  }));
  CompleteResponse(net::OK);

  // Waiting for the first data chunk is received.
  RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(state(), State::kChunkReceived);
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(client_for_fetch_handler()->state(), State::kChunkReceived);

  // Abort the consumer handle after the first data chunk has arrived.
  AbortBodyConsumerHandle();

  // Once the data pipe for RaceNetworkRequest is closed, the fetch handler side
  // data pipe is also closed.
  client_for_fetch_handler()->ConsumeChunk();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client_for_fetch_handler()->IsDisconnected());
}

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest,
       DataPipeDisconnected_FetchHandler) {
  const uint32_t data_pipe_capacity_num_bytes = 8;
  SetUpURLLoaderClient(data_pipe_capacity_num_bytes);

  // Expected input size should be larger than the data pipe size.
  const std::string kExpectedBody = "abcdefghijklmnopqrstu";
  ASSERT_GT(kExpectedBody.size(), data_pipe_capacity_num_bytes);
  WriteData(kExpectedBody);

  // Set the callback for OnCommitResponse. This callback start watching the
  // response body data pipe.
  SetOnCommitResponseCallback(base::BindOnce(
      &ServiceWorkerRaceNetworkRequestURLLoaderClientTest::WatchResponseBody,
      base::Unretained(this)));

  // Set the callback for the commit completion.
  SetOnCompletedCallback(base::BindOnce([](int error_code, const char* reason) {
    EXPECT_EQ(error_code, net::OK);
  }));
  CompleteResponse(net::OK);

  // Waiting for the first data chunk is received.
  client_for_fetch_handler()->RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(client_for_fetch_handler()->state(), State::kChunkReceived);
  RunUntilStateChange(/*resume_state=*/false);
  EXPECT_EQ(state(), State::kChunkReceived);

  // Abort the consumer handle after the first data chunk has arrived.
  client_for_fetch_handler()->AbortBodyConsumerHandle();

  // Once the data pipe for RaceNetworkRequest is closed, the fetch handler side
  // data pipe is also closed.
  ConsumeChunk();
  EXPECT_TRUE(IsDisconnected());
}

TEST_F(ServiceWorkerRaceNetworkRequestURLLoaderClientTest,
       NetworkError_AfterInitialResponse) {
  const uint32_t data_pipe_capacity_num_bytes = 8;
  SetUpURLLoaderClient(data_pipe_capacity_num_bytes);

  const std::string kExpectedBody = "abcdefghijklmnopqrstu";
  ASSERT_GT(kExpectedBody.size(), data_pipe_capacity_num_bytes);

  // Set the callback for the commit completion.
  SetOnCompletedCallback(base::BindOnce([](int error_code, const char* reason) {
    EXPECT_EQ(error_code, net::ERR_FAILED);
  }));

  // |client_| receives the response and expect |state_| is changed to
  // kResponseReceived.
  WriteData(kExpectedBody);
  EXPECT_EQ(
      client_state(),
      ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kResponseReceived);

  // Set kWithoutServiceWorker. This imitates the fetch handler fallback case.
  owner()->SetCommitResponsibility(
      ServiceWorkerRaceNetworkRequestURLLoaderClient::FetchResponseFrom::
          kWithoutServiceWorker);

  // |client_| suddenly receives the network error, and expect |state_| is
  // changed to kCompleted directly from kResponseReceived.
  CompleteResponse(net::ERR_FAILED);
  EXPECT_EQ(client_state(),
            ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kCompleted);
}
}  // namespace content
