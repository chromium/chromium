// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/sync_load_context.h"
#include "base/bind.h"
#include "base/threading/thread.h"
#include "content/renderer/loader/sync_load_response.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestSharedURLLoaderFactory : public network::TestURLLoaderFactory,
                                   public network::SharedURLLoaderFactory {
 public:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    network::TestURLLoaderFactory::CreateLoaderAndStart(
        std::move(receiver), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>) override {
    NOTREACHED();
  }

  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class base::RefCounted<TestSharedURLLoaderFactory>;
  ~TestSharedURLLoaderFactory() override = default;
};

class MockSharedURLLoaderFactoryInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  explicit MockSharedURLLoaderFactoryInfo()
      : factory_(base::MakeRefCounted<TestSharedURLLoaderFactory>()) {}

  scoped_refptr<TestSharedURLLoaderFactory> factory() const { return factory_; }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return factory_;
  }

  scoped_refptr<TestSharedURLLoaderFactory> factory_;
};

class MockResourceDispatcher : public ResourceDispatcher {
 public:
  int CreatePendingRequest(std::unique_ptr<RequestPeer> peer) {
    peers_.push_back(std::move(peer));
    return peers_.size() - 1;
  }

  bool RemovePendingRequest(
      int request_id,
      scoped_refptr<base::SingleThreadTaskRunner>) override {
    if (request_id < 0 || static_cast<int>(peers_.size()) <= request_id)
      return false;
    peers_[request_id].reset();
    return true;
  }

 private:
  std::vector<std::unique_ptr<RequestPeer>> peers_;
};

}  // namespace

class SyncLoadContextTest : public testing::Test {
 public:
  SyncLoadContextTest() : loading_thread_("loading thread") {}

  void SetUp() override {
    ASSERT_TRUE(loading_thread_.StartAndWaitForTesting());
  }

  void StartAsyncWithWaitableEventOnLoadingThread(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> factory_info,
      SyncLoadResponse* out_response,
      base::WaitableEvent* redirect_or_response_event) {
    loading_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncLoadContext::StartAsyncWithWaitableEvent,
                       std::move(request), MSG_ROUTING_NONE,
                       loading_thread_.task_runner(),
                       TRAFFIC_ANNOTATION_FOR_TESTS, std::move(factory_info),
                       std::vector<std::unique_ptr<blink::URLLoaderThrottle>>(),
                       out_response, redirect_or_response_event,
                       nullptr /* terminate_sync_load_event */,
                       base::TimeDelta::FromSeconds(60) /* timeout */,
                       mojo::NullRemote() /* download_to_blob_registry */));
  }

  static void RunSyncLoadContextViaDataPipe(
      network::ResourceRequest* request,
      SyncLoadResponse* response,
      std::string expected_data,
      base::WaitableEvent* redirect_or_response_event,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner->BelongsToCurrentThread());
    auto* context = new SyncLoadContext(
        request, std::make_unique<MockSharedURLLoaderFactoryInfo>(), response,
        redirect_or_response_event, nullptr /* terminate_sync_load_event */,
        base::TimeDelta::FromSeconds(60) /* timeout */,
        mojo::NullRemote() /* download_to_blob_registry */, task_runner);

    // Override |resource_dispatcher_| for testing.
    auto dispatcher = std::make_unique<MockResourceDispatcher>();
    context->request_id_ =
        dispatcher->CreatePendingRequest(base::WrapUnique(context));
    context->resource_dispatcher_ = std::move(dispatcher);

    // Simulate the response.
    context->OnReceivedResponse(network::mojom::URLResponseHead::New());
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr /* options */, &producer_handle,
                                   &consumer_handle));
    context->OnStartLoadingResponseBody(std::move(consumer_handle));
    context->OnCompletedRequest(network::URLLoaderCompletionStatus(net::OK));

    mojo::BlockingCopyFromString(expected_data, producer_handle);
  }

  base::Thread loading_thread_;
};

TEST_F(SyncLoadContextTest, StartAsyncWithWaitableEvent) {
  GURL expected_url = GURL("https://example.com");
  std::string expected_data = "foobarbaz";

  // Create and exercise SyncLoadContext on the |loading_thread_|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  auto factory_info = std::make_unique<MockSharedURLLoaderFactoryInfo>();
  factory_info->factory()->AddResponse(expected_url.spec(), expected_data);
  SyncLoadResponse response;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  StartAsyncWithWaitableEventOnLoadingThread(std::move(request),
                                             std::move(factory_info), &response,
                                             &redirect_or_response_event);

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  EXPECT_EQ(expected_data, response.data);
}

TEST_F(SyncLoadContextTest, ResponseBodyViaDataPipe) {
  GURL expected_url = GURL("https://example.com");
  std::string expected_data = "foobarbaz";

  // Create and exercise SyncLoadContext on the |loading_thread_|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  SyncLoadResponse response;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContextTest::RunSyncLoadContextViaDataPipe,
                     request.get(), &response, expected_data,
                     &redirect_or_response_event,
                     loading_thread_.task_runner()));

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  EXPECT_EQ(expected_data, response.data);
}

}  // namespace content
