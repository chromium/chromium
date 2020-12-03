// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_loader_client_impl.h"

#include <vector>
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/test_request_peer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

namespace {

constexpr size_t kDataPipeCapacity = 4096;

std::string ReadOneChunk(mojo::ScopedDataPipeConsumerHandle* handle) {
  char buffer[kDataPipeCapacity];
  uint32_t read_bytes = kDataPipeCapacity;
  MojoResult result =
      (*handle)->ReadData(buffer, &read_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return "";
  return std::string(buffer, read_bytes);
}

std::string GetRequestPeerContextBody(TestRequestPeer::Context* context) {
  if (context->body_handle) {
    context->data += ReadOneChunk(&context->body_handle);
  }
  return context->data;
}

}  // namespace

class URLLoaderClientImplTest : public ::testing::Test,
                                public network::mojom::URLLoaderFactory,
                                public ::testing::WithParamInterface<bool> {
 protected:
  URLLoaderClientImplTest() : dispatcher_(new ResourceDispatcher()) {
    if (DeferWithBackForwardCacheEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kLoadingTasksUnfreezable);
    }
    auto request = std::make_unique<network::ResourceRequest>();
    request_id_ = dispatcher_->StartAsync(
        std::move(request), 0 /* loader_option */,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        std::make_unique<TestRequestPeer>(dispatcher_.get(),
                                          &request_peer_context_),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<blink::URLLoaderThrottle>>(),
        std::make_unique<blink::ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr));
    request_peer_context_.request_id = request_id_;

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(url_loader_client_);
  }

  bool DeferWithBackForwardCacheEnabled() { return GetParam(); }

  void TearDown() override { url_loader_client_.reset(); }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    url_loader_client_.Bind(std::move(client));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }

  static MojoCreateDataPipeOptions DataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kDataPipeCapacity;
    return options;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
  TestRequestPeer::Context request_peer_context_;
  int request_id_ = 0;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
};

TEST_P(URLLoaderClientImplTest, OnReceiveResponse) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());

  EXPECT_FALSE(request_peer_context_.received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
}

TEST_P(URLLoaderClientImplTest, ResponseBody) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());

  EXPECT_FALSE(request_peer_context_.received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);

  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest, OnReceiveRedirect) {
  net::RedirectInfo redirect_info;

  url_loader_client_->OnReceiveRedirect(redirect_info,
                                        network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
}

TEST_P(URLLoaderClientImplTest, OnReceiveCachedMetadata) {
  std::vector<uint8_t> data;
  data.push_back('a');
  mojo_base::BigBuffer metadata(data);

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  url_loader_client_->OnReceiveCachedMetadata(std::move(metadata));

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ(0u, request_peer_context_.cached_metadata.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  ASSERT_EQ(1u, request_peer_context_.cached_metadata.size());
  EXPECT_EQ('a', request_peer_context_.cached_metadata.data()[0]);
}

TEST_P(URLLoaderClientImplTest, OnTransferSizeUpdated) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnTransferSizeUpdated(4);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ(8, request_peer_context_.total_encoded_data_length);
}

TEST_P(URLLoaderClientImplTest, OnCompleteWithResponseBody) {
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));

  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.complete);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_TRUE(request_peer_context_.complete);
}

// Due to the lack of ordering guarantee, it is possible that the response body
// bytes arrives after the completion message. URLLoaderClientImpl should
// restore the order.
TEST_P(URLLoaderClientImplTest, OnCompleteShouldBeTheLastMessage) {
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);

  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest, CancelOnReceiveResponse) {
  request_peer_context_.cancel_on_receive_response = true;

  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_TRUE(request_peer_context_.cancelled);
}

TEST_P(URLLoaderClientImplTest, Defer) {
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe;
  data_pipe.producer_handle.reset();  // Empty body.
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_P(URLLoaderClientImplTest, DeferWithResponseBody) {
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  std::string msg1 = "hello";
  uint32_t size = msg1.size();
  ASSERT_EQ(MOJO_RESULT_OK, data_pipe.producer_handle->WriteData(
                                msg1.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg1.size(), size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest,
       DeferredAndDeferredWithBackForwardCacheTransitions) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse and OnStartLoadingResponseBody while
  // deferred (not for back-forward cache).
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Write data to the response body pipe.
  std::string msg1 = "he";
  uint32_t size = msg1.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg1.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg1.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Defer for back-forward cache.
  dispatcher_->SetDefersLoading(
      request_id_,
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache);
  std::string msg2 = "ll";
  size = msg2.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg2.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg2.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Defer not for back-forward cache again.
  dispatcher_->SetDefersLoading(
      request_id_,
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache);
  std::string msg3 = "o";
  size = msg3.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg3.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg3.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Stop deferring.
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));

  // Write more data to the pipe while not deferred.
  std::string msg4 = "world";
  size = msg4.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg4.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg4.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest,
       DeferredWithBackForwardCacheStoppedDeferringBeforeClosing) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse, OnStartLoadingResponseBody, OnComplete while
  // deferred.
  dispatcher_->SetDefersLoading(
      request_id_,
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache);
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));
  network::URLLoaderCompletionStatus status;
  url_loader_client_->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Write data to the response body pipe, but don't close the connection yet.
  std::string msg1 = "hello";
  uint32_t size = msg1.size();
  // We expect that the other end of the pipe to be ready to read the data
  // immediately.
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg1.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg1.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Stop deferring. OnComplete message shouldn't be dispatched yet because
  // we're still waiting for the response body pipe to be closed.
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  // When the body is buffered, we'll wait until the pipe is closed before
  // sending the OnComplete message.
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));

  // Write more data to the pipe while not deferred.
  std::string msg2 = "world";
  size = msg2.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg2.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg2.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(&request_peer_context_));

  // Close the response body pipe.
  producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest, DeferBodyWithoutOnComplete) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  // Call OnStartLoadingResponseBody while deferred.
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Write data to the response body pipe, but don't close the connection yet.
  std::string msg1 = "hello";
  uint32_t size = msg1.size();
  // We expect that the other end of the pipe to be ready to read the data
  // immediately.
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                msg1.data(), &size, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(msg1.size(), size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Stop deferring.
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));

  // Close the response body pipe.
  producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
}

TEST_P(URLLoaderClientImplTest, DeferredWithBackForwardCacheLongResponseBody) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse, OnStartLoadingResponseBody, OnComplete while
  // deferred.
  dispatcher_->SetDefersLoading(
      request_id_,
      blink::WebURLLoader::DeferType::kDeferredWithBackForwardCache);
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));
  network::URLLoaderCompletionStatus status;
  url_loader_client_->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));

  // Write to the response body pipe. It will take several writes.
  const uint32_t body_size = 70000;
  uint32_t bytes_remaining = body_size;
  std::string body(body_size, '*');
  while (bytes_remaining > 0) {
    uint32_t start_position = body_size - bytes_remaining;
    uint32_t bytes_sent = bytes_remaining;
    MojoResult result = producer_handle->WriteData(
        body.c_str() + start_position, &bytes_sent, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
        // When we buffer the body the pipe gets drained asynchronously, so it's
        // possible to keep writing to the pipe if we wait.
        base::RunLoop().RunUntilIdle();
        continue;
    }
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_GE(bytes_remaining, bytes_sent);
    bytes_remaining -= bytes_sent;
  }
  // Ensure we've written all that we can write. When buffering is disabled, we
  // can only write |body_size| - |bytes_remaining| bytes.
  const uint32_t bytes_written = body_size - bytes_remaining;
  EXPECT_EQ(body_size, bytes_written);
  producer_handle.reset();

  // Stop deferring.
  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  // When the body is buffered, BodyBuffer shouldn't be finished writing to the
  // new response body pipe at this point (because nobody is reading it).
  EXPECT_FALSE(request_peer_context_.complete);

  // Calling GetRequestPeerContextBody to read data from the new response body
  // pipe will make BodyBuffer write the rest of the body to the pipe.
  uint32_t bytes_read = 0;
  while (bytes_read < bytes_written) {
    bytes_read = GetRequestPeerContextBody(&request_peer_context_).size();
    base::RunLoop().RunUntilIdle();
  }
  // Ensure that we've read everything we've written.
  EXPECT_EQ(bytes_written, bytes_read);
  EXPECT_EQ(body, GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_TRUE(request_peer_context_.complete);
}

// As "transfer size update" message is handled specially in the implementation,
// we have a separate test.
TEST_P(URLLoaderClientImplTest, DeferWithTransferSizeUpdated) {
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
}

TEST_P(URLLoaderClientImplTest, SetDeferredDuringFlushingDeferredMessage) {
  request_peer_context_.defer_on_redirect = true;

  net::RedirectInfo redirect_info;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveRedirect(redirect_info,
                                        network::mojom::URLResponseHead::New());
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(&request_peer_context_));
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

TEST_P(URLLoaderClientImplTest,
       SetDeferredDuringFlushingDeferredMessageOnTransferSizeUpdated) {
  request_peer_context_.defer_on_transfer_size_updated = true;

  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New());
  mojo::DataPipe data_pipe;
  data_pipe.producer_handle.reset();  // Empty body.
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));

  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kDeferred);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_,
                                blink::WebURLLoader::DeferType::kNotDeferred);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

INSTANTIATE_TEST_SUITE_P(All, URLLoaderClientImplTest, ::testing::Bool());

}  // namespace content
