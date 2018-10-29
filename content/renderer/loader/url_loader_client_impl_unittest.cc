// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_loader_client_impl.h"

#include <vector>
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/test_request_peer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

class URLLoaderClientImplTest : public ::testing::Test,
                                public network::mojom::URLLoaderFactory {
 protected:
  URLLoaderClientImplTest() : dispatcher_(new ResourceDispatcher()) {
    auto request = std::make_unique<network::ResourceRequest>();
    // Set request context type to fetch so that ResourceDispatcher doesn't
    // install MimeSniffingThrottle, which makes URLLoaderThrottleLoader
    // defer the request.
    request->fetch_request_context_type =
        static_cast<int>(blink::mojom::RequestContextType::FETCH);
    request_id_ = dispatcher_->StartAsync(
        std::move(request), 0,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false, false,
        std::make_unique<TestRequestPeer>(dispatcher_.get(),
                                          &request_peer_context_),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        nullptr /* navigation_response_override_params */,
        nullptr /* continue_navigation_function */);
    request_peer_context_.request_id = request_id_;

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(url_loader_client_);
  }

  void TearDown() override {
    url_loader_client_ = nullptr;
  }

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    url_loader_client_ = std::move(client);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    NOTREACHED();
  }

  static MojoCreateDataPipeOptions DataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 4096;
    return options;
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
  TestRequestPeer::Context request_peer_context_;
  int request_id_ = 0;
  network::mojom::URLLoaderClientPtr url_loader_client_;
};

TEST_F(URLLoaderClientImplTest, OnReceiveResponse) {
  network::ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head);

  EXPECT_FALSE(request_peer_context_.received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
}

TEST_F(URLLoaderClientImplTest, ResponseBody) {
  network::ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head);

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
  EXPECT_EQ("hello", request_peer_context_.data);
}

TEST_F(URLLoaderClientImplTest, OnReceiveRedirect) {
  network::ResourceResponseHead response_head;
  net::RedirectInfo redirect_info;

  url_loader_client_->OnReceiveRedirect(redirect_info, response_head);

  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
}

TEST_F(URLLoaderClientImplTest, OnReceiveCachedMetadata) {
  network::ResourceResponseHead response_head;
  std::vector<uint8_t> metadata;
  metadata.push_back('a');

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnReceiveCachedMetadata(metadata);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.cached_metadata.empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  ASSERT_EQ(1u, request_peer_context_.cached_metadata.size());
  EXPECT_EQ('a', request_peer_context_.cached_metadata[0]);
}

TEST_F(URLLoaderClientImplTest, OnTransferSizeUpdated) {
  network::ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnTransferSizeUpdated(4);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ(8, request_peer_context_.total_encoded_data_length);
}

TEST_F(URLLoaderClientImplTest, OnCompleteWithoutResponseBody) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, OnCompleteWithResponseBody) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
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
  EXPECT_EQ("", request_peer_context_.data);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);

  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.complete);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_TRUE(request_peer_context_.complete);
}

// Due to the lack of ordering guarantee, it is possible that the response body
// bytes arrives after the completion message. URLLoaderClientImpl should
// restore the order.
TEST_F(URLLoaderClientImplTest, OnCompleteShouldBeTheLastMessage) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);

  data_pipe.producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, CancelOnReceiveResponse) {
  request_peer_context_.cancel_on_receive_response = true;

  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
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

TEST_F(URLLoaderClientImplTest, CancelOnReceiveData) {
  request_peer_context_.cancel_on_receive_data = true;

  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_TRUE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest, Defer) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, DeferWithResponseBody) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
}

// As "transfer size update" message is handled specially in the implementation,
// we have a separate test.
TEST_F(URLLoaderClientImplTest, DeferWithTransferSizeUpdated) {
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);
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
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
}

TEST_F(URLLoaderClientImplTest, SetDeferredDuringFlushingDeferredMessage) {
  request_peer_context_.defer_on_redirect = true;

  net::RedirectInfo redirect_info;
  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveRedirect(redirect_info, response_head);
  url_loader_client_->OnReceiveResponse(response_head);
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
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest,
       SetDeferredDuringFlushingDeferredMessageOnTransferSizeUpdated) {
  request_peer_context_.defer_on_transfer_size_updated = true;

  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveResponse(response_head);

  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest, CancelOnReceiveDataWhileFlushing) {
  request_peer_context_.cancel_on_receive_data = true;
  dispatcher_->SetDefersLoading(request_id_, true);

  network::ResourceResponseHead response_head;
  network::URLLoaderCompletionStatus status;

  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  url_loader_client_->OnReceiveResponse(response_head);
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_TRUE(request_peer_context_.cancelled);
}

}  // namespace content
