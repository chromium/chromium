// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

class TestSharedURLLoaderFactory : public network::TestURLLoaderFactory,
                                   public network::SharedURLLoaderFactory {
 public:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    network::TestURLLoaderFactory::CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>) override {
    NOTREACHED();
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
  }

 private:
  friend class base::RefCounted<TestSharedURLLoaderFactory>;
  ~TestSharedURLLoaderFactory() override = default;
};

class MockPendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit MockPendingSharedURLLoaderFactory()
      : factory_(base::MakeRefCounted<TestSharedURLLoaderFactory>()) {}

  scoped_refptr<TestSharedURLLoaderFactory> factory() const { return factory_; }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return factory_;
  }

  scoped_refptr<TestSharedURLLoaderFactory> factory_;
};

class MockResourceRequestSender : public ResourceRequestSender {
 public:
  void CreatePendingRequest(scoped_refptr<ResourceRequestClient> client) {
    client_ = std::move(client);
  }

  void DeletePendingRequest(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override {
    task_runner->ReleaseSoon(FROM_HERE, std::move(client_));
  }

 private:
  scoped_refptr<ResourceRequestClient> client_;
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
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      SyncLoadResponse* out_response,
      SyncLoadContext** context_for_redirect,
      base::WaitableEvent* redirect_or_response_event) {
    loading_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncLoadContext::StartAsyncWithWaitableEvent, std::move(request),
            loading_thread_.task_runner(), TRAFFIC_ANNOTATION_FOR_TESTS,
            0 /* loader_options */, std::move(pending_factory),
            std::vector<std::unique_ptr<URLLoaderThrottle>>(), out_response,
            context_for_redirect, redirect_or_response_event,
            nullptr /* terminate_sync_load_event */,
            base::Seconds(60) /* timeout */,
            mojo::NullRemote() /* download_to_blob_registry */,
            Vector<String>() /* cors_exempt_header_list */,
            std::make_unique<ResourceLoadInfoNotifierWrapper>(
                /*resource_load_info_notifier=*/nullptr,
                task_environment_.GetMainThreadTaskRunner())));
  }

  static void RunSyncLoadContextViaDataPipe(
      network::ResourceRequest* request,
      SyncLoadResponse* response,
      SyncLoadContext** context_for_redirect,
      std::string expected_data,
      base::WaitableEvent* redirect_or_response_event,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner->BelongsToCurrentThread());
    auto context = base::AdoptRef(new SyncLoadContext(
        request, std::make_unique<MockPendingSharedURLLoaderFactory>(),
        response, context_for_redirect, redirect_or_response_event,
        nullptr /* terminate_sync_load_event */,
        base::Seconds(60) /* timeout */,
        mojo::NullRemote() /* download_to_blob_registry */, task_runner));

    auto mock_resource_request_sender =
        std::make_unique<MockResourceRequestSender>();
    mock_resource_request_sender->CreatePendingRequest(context);
    context->resource_request_sender_ = std::move(mock_resource_request_sender);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr /* options */, producer_handle,
                                   consumer_handle));

    // Simulate the response.
    context->OnReceivedResponse(network::mojom::URLResponseHead::New(),
                                std::move(consumer_handle),
                                /*cached_metadata=*/std::nullopt);
    context->OnCompletedRequest(network::URLLoaderCompletionStatus(net::OK));

    mojo::BlockingCopyFromString(expected_data, producer_handle);
  }

  static void SimulateRedirectAndPrematureCompletion(
      network::ResourceRequest* request,
      const GURL& redirect_url,
      const network::URLLoaderCompletionStatus& completion_status,
      SyncLoadResponse* response,
      SyncLoadContext** context_for_redirect,
      base::WaitableEvent* redirect_or_response_event,
      base::WaitableEvent* completion_flushed_event,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner->BelongsToCurrentThread());
    auto context = base::AdoptRef(new SyncLoadContext(
        request, std::make_unique<MockPendingSharedURLLoaderFactory>(),
        response, context_for_redirect, redirect_or_response_event,
        nullptr /* terminate_sync_load_event */,
        base::Seconds(60) /* timeout */,
        mojo::NullRemote() /* download_to_blob_registry */, task_runner));

    auto mock_resource_request_sender =
        std::make_unique<MockResourceRequestSender>();
    mock_resource_request_sender->CreatePendingRequest(context);
    context->resource_request_sender_ = std::move(mock_resource_request_sender);

    net::RedirectInfo redirect_info;
    redirect_info.new_url = redirect_url;
    context->OnReceivedRedirect(
        redirect_info, network::mojom::URLResponseHead::New(),
        base::BindOnce([](std::vector<std::string>, net::HttpRequestHeaders) {
          NOTREACHED();
        }));
    context->OnCompletedRequest(completion_status);

    // Post an extra task on `task_runner` so any premature ReleaseSoon task
    // would execute before the test thread posts FollowRedirect/CancelRedirect.
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                  base::Unretained(completion_flushed_event)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::Thread loading_thread_;
};

TEST_F(SyncLoadContextTest, StartAsyncWithWaitableEvent) {
  GURL expected_url = GURL("https://example.com");
  std::string expected_data = "foobarbaz";

  // Create and exercise SyncLoadContext on the |loading_thread_|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  auto pending_factory = std::make_unique<MockPendingSharedURLLoaderFactory>();
  pending_factory->factory()->AddResponse(expected_url.spec(), expected_data);
  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  StartAsyncWithWaitableEventOnLoadingThread(
      std::move(request), std::move(pending_factory), &response,
      &context_for_redirect, &redirect_or_response_event);

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(expected_data,
            std::string(response.data->begin()->data(), response.data->size()));
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
  SyncLoadContext* context_for_redirect = nullptr;
  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContextTest::RunSyncLoadContextViaDataPipe,
                     request.get(), &response, &context_for_redirect,
                     expected_data, &redirect_or_response_event,
                     loading_thread_.task_runner()));

  // Wait until the response is received.
  redirect_or_response_event.Wait();

  // Check if |response| is set properly after the WaitableEvent fires.
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(expected_data,
            std::string(response.data->begin()->data(), response.data->size()));
}

TEST_F(SyncLoadContextTest, RedirectCompletedBeforeFollowRedirect) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com/start");
  const GURL redirect_url("https://example.com/redirected");

  network::URLLoaderCompletionStatus status(net::ERR_CONNECTION_RESET);
  status.extended_error_code = 42;

  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent completion_flushed_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncLoadContextTest::SimulateRedirectAndPrematureCompletion,
          request.get(), redirect_url, status, &response, &context_for_redirect,
          &redirect_or_response_event, &completion_flushed_event,
          loading_thread_.task_runner()));

  redirect_or_response_event.Wait();
  completion_flushed_event.Wait();

  ASSERT_NE(nullptr, context_for_redirect);
  ASSERT_TRUE(response.redirect_info.has_value());
  EXPECT_EQ(redirect_url, response.redirect_info->new_url);

  redirect_or_response_event.Reset();
  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContext::FollowRedirect,
                     base::Unretained(context_for_redirect),
                     std::vector<std::string>(), net::HttpRequestHeaders()));
  redirect_or_response_event.Wait();

  EXPECT_EQ(nullptr, context_for_redirect);
  EXPECT_FALSE(response.redirect_info.has_value());
  EXPECT_EQ(net::ERR_CONNECTION_RESET, response.error_code);
  EXPECT_EQ(42, response.extended_error_code);
}

TEST_F(SyncLoadContextTest, RedirectCompletedBeforeCancelRedirect) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com/start");
  const GURL redirect_url("https://example.com/redirected");

  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent completion_flushed_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncLoadContextTest::SimulateRedirectAndPrematureCompletion,
          request.get(), redirect_url,
          network::URLLoaderCompletionStatus(net::ERR_FAILED), &response,
          &context_for_redirect, &redirect_or_response_event,
          &completion_flushed_event, loading_thread_.task_runner()));

  redirect_or_response_event.Wait();
  completion_flushed_event.Wait();

  ASSERT_NE(nullptr, context_for_redirect);
  redirect_or_response_event.Reset();
  loading_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SyncLoadContext::CancelRedirect,
                                base::Unretained(context_for_redirect)));
  redirect_or_response_event.Wait();

  EXPECT_EQ(nullptr, context_for_redirect);
  EXPECT_FALSE(response.redirect_info.has_value());
  EXPECT_EQ(net::ERR_ABORTED, response.error_code);
}

TEST_F(SyncLoadContextTest, RedirectCompletedWithUnexpectedOkBeforeFollow) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://example.com/start");
  const GURL redirect_url("https://example.com/redirected");

  SyncLoadResponse response;
  SyncLoadContext* context_for_redirect = nullptr;
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent completion_flushed_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncLoadContextTest::SimulateRedirectAndPrematureCompletion,
          request.get(), redirect_url,
          network::URLLoaderCompletionStatus(net::OK), &response,
          &context_for_redirect, &redirect_or_response_event,
          &completion_flushed_event, loading_thread_.task_runner()));

  redirect_or_response_event.Wait();
  completion_flushed_event.Wait();

  ASSERT_NE(nullptr, context_for_redirect);
  redirect_or_response_event.Reset();
  loading_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncLoadContext::FollowRedirect,
                     base::Unretained(context_for_redirect),
                     std::vector<std::string>(), net::HttpRequestHeaders()));
  redirect_or_response_event.Wait();

  EXPECT_EQ(nullptr, context_for_redirect);
  EXPECT_FALSE(response.redirect_info.has_value());
  EXPECT_EQ(net::ERR_FAILED, response.error_code);
}

}  // namespace blink
