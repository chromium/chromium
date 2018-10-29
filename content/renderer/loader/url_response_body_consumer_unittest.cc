// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_response_body_consumer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

class TestRequestPeer : public RequestPeer {
 public:
  struct Context;
  TestRequestPeer(Context* context,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : context_(context), task_runner_(std::move(task_runner)) {}

  void OnUploadProgress(uint64_t position, uint64_t size) override {
    ADD_FAILURE() << "OnUploadProgress should not be called.";
  }

  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseInfo& info) override {
    ADD_FAILURE() << "OnReceivedRedirect should not be called.";
    return false;
  }

  void OnReceivedResponse(const network::ResourceResponseInfo& info) override {
    ADD_FAILURE() << "OnReceivedResponse should not be called.";
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    ADD_FAILURE() << "OnStartLoadingResponseBody should not be called.";
  }

  void OnReceivedData(std::unique_ptr<ReceivedData> data) override {
    EXPECT_FALSE(context_->complete);
    context_->data.append(data->payload(), data->length());
    if (context_->release_data_asynchronously)
      task_runner_->DeleteSoon(FROM_HERE, data.release());
    context_->run_loop_quit_closure.Run();
  }

  void OnTransferSizeUpdated(int transfer_size_diff) override {}

  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    EXPECT_FALSE(context_->complete);
    context_->complete = true;
    context_->error_code = status.error_code;
    context_->run_loop_quit_closure.Run();
  }

  struct Context {
    // Data received. If downloading to file, remains empty.
    std::string data;
    bool complete = false;
    base::Closure run_loop_quit_closure;
    int error_code = net::OK;
    bool release_data_asynchronously = false;
  };

 private:
  Context* context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestPeer);
};

class URLResponseBodyConsumerTest : public ::testing::Test {
 protected:
  // A URLResponseBodyConsumer needs an associated PendingRequestInfo, and
  // we need a URLLoaderFactory to create a PendingRequestInfo. We don't need
  // a true URLLoaderFactory, so here we define a no-op (other than keeping
  // clients to avoid connection error notifications) factory.
  class NoopURLLoaderFactory final : public network::mojom::URLLoaderFactory {
   public:
    void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                              int32_t routing_id,
                              int32_t request_id,
                              uint32_t options,
                              const network::ResourceRequest& url_request,
                              network::mojom::URLLoaderClientPtr client,
                              const net::MutableNetworkTrafficAnnotationTag&
                                  traffic_annotation) override {
      clients_.push_back(std::move(client));
    }

    void Clone(network::mojom::URLLoaderFactoryRequest request) override {}

    std::vector<network::mojom::URLLoaderClientPtr> clients_;
  };

  URLResponseBodyConsumerTest() : dispatcher_(new ResourceDispatcher()) {}

  ~URLResponseBodyConsumerTest() override {
    dispatcher_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
    std::unique_ptr<network::ResourceRequest> request(
        new network::ResourceRequest);

    request->method = "GET";
    request->url = GURL("http://www.example.com/");
    request->priority = net::LOW;
    request->appcache_host_id = 0;
    request->fetch_request_mode = network::mojom::FetchRequestMode::kNoCORS;
    request->fetch_frame_type = network::mojom::RequestContextFrameType::kNone;

    const RequestExtraData extra_data;
    extra_data.CopyToResourceRequest(request.get());

    return request;
  }

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  // Returns the request id.
  int SetUpRequestPeer(std::unique_ptr<network::ResourceRequest> request,
                       TestRequestPeer::Context* context) {
    return dispatcher_->StartAsync(
        std::move(request), 0,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        false /* pass_response_pipe_to_peer */,
        std::make_unique<TestRequestPeer>(
            context, blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &factory_),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        nullptr /* navigation_response_override_params */,
        nullptr /* continue_navigation_function */);
  }

  void Run(TestRequestPeer::Context* context) {
    base::RunLoop run_loop;
    context->run_loop_quit_closure = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  NoopURLLoaderFactory factory_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
  static const MojoWriteDataFlags kNone = MOJO_WRITE_DATA_FLAG_NONE;
};

TEST_F(URLResponseBodyConsumerTest, ReceiveData) {
  TestRequestPeer::Context context;
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  int request_id = SetUpRequestPeer(std::move(request), &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  consumer->ArmOrNotify();

  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  std::string buffer = "hello";
  uint32_t size = buffer.size();
  MojoResult result = writer->WriteData(buffer.c_str(), &size, kNone);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(buffer.size(), size);

  Run(&context);

  EXPECT_FALSE(context.complete);
  EXPECT_EQ("hello", context.data);
}

TEST_F(URLResponseBodyConsumerTest, OnCompleteThenClose) {
  TestRequestPeer::Context context;
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  int request_id = SetUpRequestPeer(std::move(request), &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  consumer->ArmOrNotify();

  consumer->OnComplete(network::URLLoaderCompletionStatus());
  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  std::string buffer = "hello";
  uint32_t size = buffer.size();
  MojoResult result = writer->WriteData(buffer.c_str(), &size, kNone);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(buffer.size(), size);

  Run(&context);

  writer.reset();
  EXPECT_FALSE(context.complete);
  EXPECT_EQ("hello", context.data);

  Run(&context);

  EXPECT_TRUE(context.complete);
  EXPECT_EQ("hello", context.data);
}

// Release the received data asynchronously. This leads to MOJO_RESULT_BUSY
// from the BeginReadDataRaw call in OnReadable.
TEST_F(URLResponseBodyConsumerTest, OnCompleteThenCloseWithAsyncRelease) {
  TestRequestPeer::Context context;
  context.release_data_asynchronously = true;
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  int request_id = SetUpRequestPeer(std::move(request), &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  consumer->ArmOrNotify();

  consumer->OnComplete(network::URLLoaderCompletionStatus());
  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  std::string buffer = "hello";
  uint32_t size = buffer.size();
  MojoResult result = writer->WriteData(buffer.c_str(), &size, kNone);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(buffer.size(), size);

  Run(&context);

  writer.reset();
  EXPECT_FALSE(context.complete);
  EXPECT_EQ("hello", context.data);

  Run(&context);

  EXPECT_TRUE(context.complete);
  EXPECT_EQ("hello", context.data);
}

TEST_F(URLResponseBodyConsumerTest, CloseThenOnComplete) {
  TestRequestPeer::Context context;
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  int request_id = SetUpRequestPeer(std::move(request), &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  consumer->ArmOrNotify();

  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  data_pipe.producer_handle.reset();
  consumer->OnComplete(status);

  Run(&context);

  EXPECT_TRUE(context.complete);
  EXPECT_EQ(net::ERR_FAILED, context.error_code);
  EXPECT_EQ("", context.data);
}

TEST_F(URLResponseBodyConsumerTest, TooBigChunkShouldBeSplit) {
  constexpr auto kMaxNumConsumedBytesInTask =
      URLResponseBodyConsumer::kMaxNumConsumedBytesInTask;
  TestRequestPeer::Context context;
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  int request_id = SetUpRequestPeer(std::move(request), &context);
  auto options = CreateDataPipeOptions();
  options.capacity_num_bytes = 2 * kMaxNumConsumedBytesInTask;
  mojo::DataPipe data_pipe(options);

  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  void* buffer = nullptr;
  uint32_t size = 0;
  MojoResult result = writer->BeginWriteData(&buffer, &size, kNone);

  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(options.capacity_num_bytes, size);

  memset(buffer, 'a', kMaxNumConsumedBytesInTask);
  memset(static_cast<char*>(buffer) + kMaxNumConsumedBytesInTask, 'b',
         kMaxNumConsumedBytesInTask);

  result = writer->EndWriteData(size);
  ASSERT_EQ(MOJO_RESULT_OK, result);

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  consumer->ArmOrNotify();

  Run(&context);
  EXPECT_EQ(std::string(kMaxNumConsumedBytesInTask, 'a'), context.data);
  context.data.clear();

  Run(&context);
  EXPECT_EQ(std::string(kMaxNumConsumedBytesInTask, 'b'), context.data);
}

}  // namespace

}  // namespace content
