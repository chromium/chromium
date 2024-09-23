// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/mojo_url_loader_client.h"

#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

constexpr size_t kDataPipeCapacity = 4096;

class MockResourceRequestSender : public ResourceRequestSender {
 public:
  struct Context;
  MockResourceRequestSender() : context_(new Context()) {}
  ~MockResourceRequestSender() override = default;

  void OnUploadProgress(int64_t position, int64_t size) override {
    EXPECT_FALSE(context_->complete);
  }

  void OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          base::TimeTicks redirect_ipc_arrival_time) override {
    EXPECT_FALSE(context_->cancelled);
    EXPECT_FALSE(context_->complete);
    ++context_->seen_redirects;
    context_->last_load_timing = head->load_timing;
    if (context_->defer_on_redirect) {
      context_->url_laoder_client->Freeze(LoaderFreezeMode::kStrict);
    }
  }

  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          mojo::ScopedDataPipeConsumerHandle body,
                          std::optional<mojo_base::BigBuffer> cached_metadata,
                          base::TimeTicks response_ipc_arrival_time) override {
    EXPECT_FALSE(context_->cancelled);
    EXPECT_FALSE(context_->received_response);
    EXPECT_FALSE(context_->complete);
    context_->received_response = true;
    context_->last_load_timing = head->load_timing;
    if (cached_metadata) {
      context_->cached_metadata = std::move(*cached_metadata);
    }
    if (context_->cancel_on_receive_response) {
      context_->cancelled = true;
      return;
    }
    context_->body_handle = std::move(body);
  }

  void OnTransferSizeUpdated(int transfer_size_diff) override {
    EXPECT_TRUE(context_->received_response);
    EXPECT_FALSE(context_->complete);
    if (context_->cancelled)
      return;
    context_->total_encoded_data_length += transfer_size_diff;
    if (context_->defer_on_transfer_size_updated) {
      context_->url_laoder_client->Freeze(LoaderFreezeMode::kStrict);
    }
  }

  void OnRequestComplete(const network::URLLoaderCompletionStatus& status,
                         base::TimeTicks complete_ipc_arrival_time) override {
    if (context_->cancelled)
      return;
    EXPECT_TRUE(context_->received_response);
    EXPECT_FALSE(context_->complete);
    context_->complete = true;
    context_->completion_status = status;
  }

  Context* context() { return context_.get(); }

  struct Context final {
    Context() = default;
    ~Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // True if should follow redirects, false if should cancel them.
    bool follow_redirects = true;
    // True if the request should be deferred on redirects.
    bool defer_on_redirect = false;

    // Number of total redirects seen.
    int seen_redirects = 0;

    bool cancel_on_receive_response = false;
    bool cancel_on_receive_data = false;
    bool received_response = false;

    mojo_base::BigBuffer cached_metadata;
    // Data received. If downloading to file, remains empty.
    std::string data;

    // Mojo's data pipe passed on OnStartLoadingResponseBody.
    mojo::ScopedDataPipeConsumerHandle body_handle;

    // Total encoded data length, regardless of whether downloading to a file or
    // not.
    int total_encoded_data_length = 0;
    bool defer_on_transfer_size_updated = false;

    bool complete = false;
    bool cancelled = false;
    int request_id = -1;

    net::LoadTimingInfo last_load_timing;
    network::URLLoaderCompletionStatus completion_status;
    raw_ptr<MojoURLLoaderClient> url_laoder_client;
  };

 private:
  std::unique_ptr<Context> context_;
};

std::string ReadOneChunk(mojo::ScopedDataPipeConsumerHandle* handle) {
  std::string buffer(kDataPipeCapacity, '\0');
  size_t actually_read_bytes = 0;
  MojoResult result = (*handle)->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                          base::as_writable_byte_span(buffer),
                                          actually_read_bytes);
  if (result != MOJO_RESULT_OK)
    return "";
  return buffer.substr(0, actually_read_bytes);
}

std::string GetRequestPeerContextBody(
    MockResourceRequestSender::Context* context) {
  if (context->body_handle) {
    context->data += ReadOneChunk(&context->body_handle);
  }
  return context->data;
}

class TestBackForwardCacheLoaderHelper : public BackForwardCacheLoaderHelper {
 public:
  TestBackForwardCacheLoaderHelper() = default;
  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason reason) override {}

  void DidBufferLoadWhileInBackForwardCache(bool update_process_wide_count,
                                            size_t num_bytes) override {}

  void Detach() override {}
};

}  // namespace

class WebMojoURLLoaderClientTest : public ::testing::Test,
                                   public network::mojom::URLLoaderFactory,
                                   public ::testing::WithParamInterface<bool> {
 protected:
  WebMojoURLLoaderClientTest()
      : resource_request_sender_(new MockResourceRequestSender()) {
    if (DeferWithBackForwardCacheEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kLoadingTasksUnfreezable);
    }

    WebRuntimeFeatures::EnableBackForwardCache(
        DeferWithBackForwardCacheEnabled());

    auto url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this);
    auto request = std::make_unique<network::ResourceRequest>();
    auto loading_task_runner =
        blink::scheduler::GetSingleThreadTaskRunnerForTesting();

    client_ = std::make_unique<MojoURLLoaderClient>(
        resource_request_sender_.get(), loading_task_runner,
        url_loader_factory->BypassRedirectChecks(), request->url,
        /*evict_from_bfcache_callback=*/
        base::OnceCallback<void(mojom::blink::RendererEvictionReason)>(),
        /*did_buffer_load_while_in_bfcache_callback=*/
        base::RepeatingCallback<void(size_t)>());
    context_ = resource_request_sender_->context();
    context_->url_laoder_client = client_.get();
    url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        std::move(url_loader_factory),
        std::vector<std::unique_ptr<blink::URLLoaderThrottle>>(), request_id_,
        /*loader_options=0*/ 0, request.get(), client_.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS, std::move(loading_task_runner),
        std::make_optional(std::vector<std::string>()));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(url_loader_client_);
  }

  bool DeferWithBackForwardCacheEnabled() { return GetParam(); }

  void TearDown() override { url_loader_client_.reset(); }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
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
    NOTREACHED_IN_MIGRATION();
  }

  static MojoCreateDataPipeOptions DataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kDataPipeCapacity;
    return options;
  }

  class TestPlatform final : public TestingPlatformSupport {
   public:
    bool IsRedirectSafe(const GURL& from_url, const GURL& to_url) override {
      return true;
    }
  };

  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestPlatform> platform_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ThrottlingURLLoader> url_loader_;
  std::unique_ptr<MojoURLLoaderClient> client_;
  std::unique_ptr<MockResourceRequestSender> resource_request_sender_;
  raw_ptr<MockResourceRequestSender::Context> context_;
  int request_id_ = 0;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
};

TEST_P(WebMojoURLLoaderClientTest, OnReceiveResponse) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        mojo::ScopedDataPipeConsumerHandle(),
                                        std::nullopt);

  EXPECT_FALSE(context_->received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
}

TEST_P(WebMojoURLLoaderClientTest, ResponseBody) {
  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);

  EXPECT_FALSE(context_->received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);

  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_producer->WriteData(
      base::byte_span_from_cstring("hello"), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, actually_written_bytes);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
}

TEST_P(WebMojoURLLoaderClientTest, OnReceiveRedirect) {
  net::RedirectInfo redirect_info;

  url_loader_client_->OnReceiveRedirect(redirect_info,
                                        network::mojom::URLResponseHead::New());

  EXPECT_EQ(0, context_->seen_redirects);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, context_->seen_redirects);
}

TEST_P(WebMojoURLLoaderClientTest, OnReceiveResponseWithCachedMetadata) {
  std::vector<uint8_t> data;
  data.push_back('a');
  mojo_base::BigBuffer metadata(data);

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        mojo::ScopedDataPipeConsumerHandle(),
                                        std::move(metadata));

  EXPECT_FALSE(context_->received_response);
  EXPECT_EQ(0u, context_->cached_metadata.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  ASSERT_EQ(1u, context_->cached_metadata.size());
  EXPECT_EQ('a', context_->cached_metadata.data()[0]);
}

TEST_P(WebMojoURLLoaderClientTest, OnTransferSizeUpdated) {
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        mojo::ScopedDataPipeConsumerHandle(),
                                        std::nullopt);
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnTransferSizeUpdated(4);

  EXPECT_FALSE(context_->received_response);
  EXPECT_EQ(0, context_->total_encoded_data_length);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_EQ(8, context_->total_encoded_data_length);
}

TEST_P(WebMojoURLLoaderClientTest, OnCompleteWithResponseBody) {
  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_producer->WriteData(
      base::byte_span_from_cstring("hello"), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, actually_written_bytes);
  data_pipe_producer.reset();

  EXPECT_FALSE(context_->received_response);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));

  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->complete);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->received_response);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
  EXPECT_TRUE(context_->complete);
}

// Due to the lack of ordering guarantee, it is possible that the response body
// bytes arrives after the completion message. URLLoaderClientImpl should
// restore the order.
TEST_P(WebMojoURLLoaderClientTest, OnCompleteShouldBeTheLastMessage) {
  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);

  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_producer->WriteData(
      base::byte_span_from_cstring("hello"), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, actually_written_bytes);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
}

TEST_P(WebMojoURLLoaderClientTest, CancelOnReceiveResponse) {
  context_->cancel_on_receive_response = true;

  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_FALSE(context_->cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_TRUE(context_->cancelled);
}

TEST_P(WebMojoURLLoaderClientTest, Defer) {
  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  data_pipe_producer.reset();  // Empty body.
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);

  client_->Freeze(LoaderFreezeMode::kStrict);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);

  client_->Freeze(LoaderFreezeMode::kNone);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
}

TEST_P(WebMojoURLLoaderClientTest, DeferWithResponseBody) {
  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  std::string msg1 = "hello";
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            data_pipe_producer->WriteData(base::as_byte_span(msg1),
                                          MOJO_WRITE_DATA_FLAG_NONE,
                                          actually_written_bytes));
  EXPECT_EQ(msg1.size(), actually_written_bytes);
  data_pipe_producer.reset();

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  client_->Freeze(LoaderFreezeMode::kStrict);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  client_->Freeze(LoaderFreezeMode::kNone);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
}

TEST_P(WebMojoURLLoaderClientTest,
       DeferredAndDeferredWithBackForwardCacheTransitions) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse while deferred (not for back-forward cache).
  client_->Freeze(LoaderFreezeMode::kStrict);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(consumer_handle),
                                        std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Write data to the response body pipe.
  std::string msg1 = "he";
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg1),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg1.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Defer for back-forward cache.
  client_->Freeze(LoaderFreezeMode::kBufferIncoming);
  std::string msg2 = "ll";
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg2),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg2.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Defer not for back-forward cache again.
  client_->Freeze(LoaderFreezeMode::kBufferIncoming);
  std::string msg3 = "o";
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg3),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg3.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Stop deferring.
  client_->Freeze(LoaderFreezeMode::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));

  // Write more data to the pipe while not deferred.
  std::string msg4 = "world";
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg4),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg4.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(context_));
}

TEST_P(WebMojoURLLoaderClientTest,
       DeferredWithBackForwardCacheStoppedDeferringBeforeClosing) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse, OnComplete while deferred.
  client_->Freeze(LoaderFreezeMode::kBufferIncoming);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(consumer_handle),
                                        std::nullopt);
  network::URLLoaderCompletionStatus status;
  url_loader_client_->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Write data to the response body pipe, but don't close the connection yet.
  std::string msg1 = "hello";
  size_t actually_written_bytes = 0;
  // We expect that the other end of the pipe to be ready to read the data
  // immediately.
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg1),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg1.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Stop deferring. OnComplete message shouldn't be dispatched yet because
  // we're still waiting for the response body pipe to be closed.
  client_->Freeze(LoaderFreezeMode::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  // When the body is buffered, we'll wait until the pipe is closed before
  // sending the OnComplete message.
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));

  // Write more data to the pipe while not deferred.
  std::string msg2 = "world";
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(msg2),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(msg2.size(), actually_written_bytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(context_));

  // Close the response body pipe.
  producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
  EXPECT_EQ("helloworld", GetRequestPeerContextBody(context_));
}

TEST_P(WebMojoURLLoaderClientTest,
       DeferredWithBackForwardCacheLongResponseBody) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  // Call OnReceiveResponse, OnComplete while deferred.
  client_->Freeze(LoaderFreezeMode::kBufferIncoming);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle));
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(consumer_handle),
                                        std::nullopt);
  network::URLLoaderCompletionStatus status;
  url_loader_client_->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));

  // Write to the response body pipe. It will take several writes.
  const size_t body_size = 70000;
  size_t bytes_remaining = body_size;
  std::string body(body_size, '*');
  while (bytes_remaining > 0) {
    base::span<const uint8_t> bytes = base::as_byte_span(body);
    bytes = bytes.last(bytes_remaining);

    size_t actually_written_bytes = 0;
    MojoResult result = producer_handle->WriteData(
        bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // When we buffer the body the pipe gets drained asynchronously, so it's
      // possible to keep writing to the pipe if we wait.
      base::RunLoop().RunUntilIdle();
      continue;
    }
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_GE(bytes_remaining, actually_written_bytes);
    bytes_remaining -= actually_written_bytes;
  }
  // Ensure we've written all that we can write. When buffering is disabled, we
  // can only write |body_size| - |bytes_remaining| bytes.
  const size_t bytes_written = body_size - bytes_remaining;
  EXPECT_EQ(body_size, bytes_written);
  producer_handle.reset();

  // Stop deferring.
  client_->Freeze(LoaderFreezeMode::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  // When the body is buffered, BodyBuffer shouldn't be finished writing to the
  // new response body pipe at this point (because nobody is reading it).
  EXPECT_FALSE(context_->complete);

  // Calling GetRequestPeerContextBody to read data from the new response body
  // pipe will make BodyBuffer write the rest of the body to the pipe.
  size_t bytes_read = 0;
  while (bytes_read < bytes_written) {
    bytes_read = GetRequestPeerContextBody(context_).size();
    base::RunLoop().RunUntilIdle();
  }
  // Ensure that we've read everything we've written.
  EXPECT_EQ(bytes_written, bytes_read);
  EXPECT_EQ(body, GetRequestPeerContextBody(context_));
  EXPECT_TRUE(context_->complete);
}

// As "transfer size update" message is handled specially in the implementation,
// we have a separate test.
TEST_P(WebMojoURLLoaderClientTest, DeferWithTransferSizeUpdated) {
  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_producer->WriteData(
      base::byte_span_from_cstring("hello"), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, actually_written_bytes);
  data_pipe_producer.reset();

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kStrict);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kNone);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
  EXPECT_EQ(4, context_->total_encoded_data_length);
}

TEST_P(WebMojoURLLoaderClientTest, SetDeferredDuringFlushingDeferredMessage) {
  context_->defer_on_redirect = true;

  net::RedirectInfo redirect_info;
  network::URLLoaderCompletionStatus status;

  url_loader_client_->OnReceiveRedirect(redirect_info,
                                        network::mojom::URLResponseHead::New());
  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_producer->WriteData(
      base::byte_span_from_cstring("hello"), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, actually_written_bytes);
  data_pipe_producer.reset();

  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_EQ(0, context_->seen_redirects);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kStrict);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, context_->seen_redirects);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kNone);
  EXPECT_EQ(0, context_->seen_redirects);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, context_->seen_redirects);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ("", GetRequestPeerContextBody(context_));
  EXPECT_EQ(0, context_->total_encoded_data_length);
  EXPECT_FALSE(context_->cancelled);

  client_->Freeze(LoaderFreezeMode::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, context_->seen_redirects);
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
  EXPECT_EQ("hello", GetRequestPeerContextBody(context_));
  EXPECT_EQ(4, context_->total_encoded_data_length);
  EXPECT_FALSE(context_->cancelled);
}

TEST_P(WebMojoURLLoaderClientTest,
       SetDeferredDuringFlushingDeferredMessageOnTransferSizeUpdated) {
  context_->defer_on_transfer_size_updated = true;

  network::URLLoaderCompletionStatus status;

  MojoCreateDataPipeOptions options = DataPipeOptions();
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
  data_pipe_producer.reset();  // Empty body.
  url_loader_client_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                        std::move(data_pipe_consumer),
                                        std::nullopt);

  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(status);

  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kStrict);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ(0, context_->total_encoded_data_length);

  client_->Freeze(LoaderFreezeMode::kNone);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ(0, context_->total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  EXPECT_EQ(4, context_->total_encoded_data_length);
  EXPECT_FALSE(context_->cancelled);

  client_->Freeze(LoaderFreezeMode::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->received_response);
  EXPECT_TRUE(context_->complete);
  EXPECT_EQ(4, context_->total_encoded_data_length);
  EXPECT_FALSE(context_->cancelled);
}

INSTANTIATE_TEST_SUITE_P(All, WebMojoURLLoaderClientTest, ::testing::Bool());

}  // namespace blink
