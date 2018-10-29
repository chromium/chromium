// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/resource_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/common/appcache_interfaces.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/fixed_received_data.h"
#include "content/public/renderer/request_peer.h"
#include "content/public/renderer/resource_dispatcher_delegate.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/test_request_peer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "url/gurl.h"

namespace content {

static constexpr char kTestPageUrl[] = "http://www.google.com/";
static constexpr char kTestPageHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static constexpr char kTestPageMimeType[] = "text/html";
static constexpr char kTestPageCharset[] = "";
static constexpr char kTestPageContents[] =
    "<html><head><title>Google</title></head><body><h1>Google</h1></body></"
    "html>";

// Sets up the message sender override for the unit test.
class ResourceDispatcherTest : public testing::Test,
                               public network::mojom::URLLoaderFactory {
 public:
  ResourceDispatcherTest() : dispatcher_(new ResourceDispatcher()) {}

  ~ResourceDispatcherTest() override {
    dispatcher_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& annotation) override {
    loader_and_clients_.emplace_back(std::move(request), std::move(client));
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    NOTREACHED();
  }

  void CallOnReceiveResponse(network::mojom::URLLoaderClient* client) {
    network::ResourceResponseHead head;
    std::string raw_headers(kTestPageHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head.headers = new net::HttpResponseHeaders(raw_headers);
    head.mime_type = kTestPageMimeType;
    head.charset = kTestPageCharset;
    client->OnReceiveResponse(head);
  }

  std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
    std::unique_ptr<network::ResourceRequest> request(
        new network::ResourceRequest());

    request->method = "GET";
    request->url = GURL(kTestPageUrl);
    request->site_for_cookies = GURL(kTestPageUrl);
    request->referrer_policy = Referrer::GetDefaultReferrerPolicy();
    request->resource_type = RESOURCE_TYPE_SUB_RESOURCE;
    request->priority = net::LOW;
    request->fetch_request_mode = network::mojom::FetchRequestMode::kNoCORS;
    request->fetch_frame_type = network::mojom::RequestContextFrameType::kNone;

    const RequestExtraData extra_data;
    extra_data.CopyToResourceRequest(request.get());

    return request;
  }

  ResourceDispatcher* dispatcher() { return dispatcher_.get(); }

  int StartAsync(std::unique_ptr<network::ResourceRequest> request,
                 network::ResourceRequestBody* request_body,
                 TestRequestPeer::Context* peer_context) {
    std::unique_ptr<TestRequestPeer> peer(
        new TestRequestPeer(dispatcher(), peer_context));
    int request_id = dispatcher()->StartAsync(
        std::move(request), 0,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false, false, std::move(peer),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        nullptr /* navigation_response_override_params */,
        nullptr /* continue_navigation_function */);
    peer_context->request_id = request_id;
    return request_id;
  }

 protected:
  std::vector<std::pair<network::mojom::URLLoaderRequest,
                        network::mojom::URLLoaderClientPtr>>
      loader_and_clients_;
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
};

// Tests the generation of unique request ids.
TEST_F(ResourceDispatcherTest, MakeRequestID) {
  int first_id = ResourceDispatcher::MakeRequestID();
  int second_id = ResourceDispatcher::MakeRequestID();

  // Child process ids are unique (per process) and counting from 0 upwards:
  EXPECT_GT(second_id, first_id);
  EXPECT_GE(first_id, 0);
}

class TestResourceDispatcherDelegate : public ResourceDispatcherDelegate {
 public:
  TestResourceDispatcherDelegate() {}
  ~TestResourceDispatcherDelegate() override {}

  std::unique_ptr<RequestPeer> OnRequestComplete(
      std::unique_ptr<RequestPeer> current_peer,
      ResourceType resource_type,
      int error_code) override {
    return current_peer;
  }

  std::unique_ptr<RequestPeer> OnReceivedResponse(
      std::unique_ptr<RequestPeer> current_peer,
      const std::string& mime_type,
      const GURL& url) override {
    return std::make_unique<WrapperPeer>(std::move(current_peer));
  }

  class WrapperPeer : public RequestPeer {
   public:
    explicit WrapperPeer(std::unique_ptr<RequestPeer> original_peer)
        : original_peer_(std::move(original_peer)) {}

    void OnUploadProgress(uint64_t position, uint64_t size) override {}

    bool OnReceivedRedirect(
        const net::RedirectInfo& redirect_info,
        const network::ResourceResponseInfo& info) override {
      return false;
    }

    void OnReceivedResponse(
        const network::ResourceResponseInfo& info) override {
      response_info_ = info;
    }

    void OnStartLoadingResponseBody(
        mojo::ScopedDataPipeConsumerHandle body) override {}

    void OnReceivedData(std::unique_ptr<ReceivedData> data) override {
      data_.append(data->payload(), data->length());
    }
    void OnTransferSizeUpdated(int transfer_size_diff) override {}

    void OnCompletedRequest(
        const network::URLLoaderCompletionStatus& status) override {
      original_peer_->OnReceivedResponse(response_info_);
      if (!data_.empty()) {
        original_peer_->OnReceivedData(
            std::make_unique<FixedReceivedData>(data_.data(), data_.size()));
      }
      original_peer_->OnCompletedRequest(status);
    }

   private:
    std::unique_ptr<RequestPeer> original_peer_;
    network::ResourceResponseInfo response_info_;
    std::string data_;

    DISALLOW_COPY_AND_ASSIGN(WrapperPeer);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherDelegate);
};

TEST_F(ResourceDispatcherTest, DelegateTest) {
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), nullptr, &peer_context);

  ASSERT_EQ(1u, loader_and_clients_.size());
  network::mojom::URLLoaderClientPtr client =
      std::move(loader_and_clients_[0].second);
  loader_and_clients_.clear();

  // Set the delegate that inserts a new peer in OnReceivedResponse.
  TestResourceDispatcherDelegate delegate;
  dispatcher()->set_delegate(&delegate);

  // The wrapper eats all messages until RequestComplete message is sent.
  CallOnReceiveResponse(client.get());

  mojo::DataPipe data_pipe;
  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  uint32_t size = strlen(kTestPageContents);
  auto result = data_pipe.producer_handle->WriteData(kTestPageContents, &size,
                                                     MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(size, strlen(kTestPageContents));

  data_pipe.producer_handle.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(peer_context.received_response);

  // This lets the wrapper peer pass all the messages to the original
  // peer at once.
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  status.exists_in_cache = false;
  status.encoded_data_length = strlen(kTestPageContents);
  client->OnComplete(status);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(peer_context.received_response);
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
}

TEST_F(ResourceDispatcherTest, CancelDuringCallbackWithWrapperPeer) {
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), nullptr, &peer_context);
  peer_context.cancel_on_receive_response = true;

  ASSERT_EQ(1u, loader_and_clients_.size());
  network::mojom::URLLoaderClientPtr client =
      std::move(loader_and_clients_[0].second);
  loader_and_clients_.clear();

  // Set the delegate that inserts a new peer in OnReceivedResponse.
  TestResourceDispatcherDelegate delegate;
  dispatcher()->set_delegate(&delegate);

  CallOnReceiveResponse(client.get());
  mojo::DataPipe data_pipe;
  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  uint32_t size = strlen(kTestPageContents);
  auto result = data_pipe.producer_handle->WriteData(kTestPageContents, &size,
                                                     MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(size, strlen(kTestPageContents));
  data_pipe.producer_handle.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(peer_context.received_response);

  // This lets the wrapper peer pass all the messages to the original
  // peer at once, but the original peer cancels right after it receives
  // the response. (This will remove pending request info from
  // ResourceDispatcher while the wrapper peer is still running
  // OnCompletedRequest, but it should not lead to crashes.)
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  status.exists_in_cache = false;
  status.encoded_data_length = strlen(kTestPageContents);
  client->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(peer_context.received_response);
  // Request should have been cancelled with no additional messages.
  EXPECT_TRUE(peer_context.cancelled);
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
}

TEST_F(ResourceDispatcherTest, Cookies) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, SerializedPostData) {
  // FIXME
}

class TimeConversionTest : public ResourceDispatcherTest {
 public:
  void PerformTest(const network::ResourceResponseHead& response_head) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    TestRequestPeer::Context peer_context;
    StartAsync(std::move(request), nullptr, &peer_context);

    ASSERT_EQ(1u, loader_and_clients_.size());
    auto client = std::move(loader_and_clients_[0].second);
    loader_and_clients_.clear();
    client->OnReceiveResponse(response_head);
  }

  const network::ResourceResponseInfo& response_info() const {
    return response_info_;
  }

 private:
  network::ResourceResponseInfo response_info_;
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  network::ResourceResponseHead response_head;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);
  response_head.load_timing.request_start_time = base::Time::Now();
  response_head.load_timing.request_start =
      base::TimeTicks::FromInternalValue(10);
  response_head.load_timing.connect_timing.connect_start =
      base::TimeTicks::FromInternalValue(13);

  PerformTest(response_head);

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
  EXPECT_LE(response_head.load_timing.request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  network::ResourceResponseHead response_head;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  network::ResourceResponseHead response_head;

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

class CompletionTimeConversionTest : public ResourceDispatcherTest {
 public:
  void PerformTest(base::TimeTicks remote_request_start,
                   base::TimeTicks completion_time,
                   base::TimeDelta delay) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    StartAsync(std::move(request), nullptr, &peer_context_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    auto client = std::move(loader_and_clients_[0].second);
    network::ResourceResponseHead response_head;
    response_head.request_start = remote_request_start;
    response_head.load_timing.request_start = remote_request_start;
    response_head.load_timing.receive_headers_end = remote_request_start;
    // We need to put somthing non-null time, otherwise no values will be
    // copied.
    response_head.load_timing.request_start_time =
        base::Time() + base::TimeDelta::FromSeconds(99);
    client->OnReceiveResponse(response_head);

    network::URLLoaderCompletionStatus status;
    status.completion_time = completion_time;

    client->OnComplete(status);

    const base::TimeTicks until = base::TimeTicks::Now() + delay;
    while (base::TimeTicks::Now() < until)
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    base::RunLoop().RunUntilIdle();
    loader_and_clients_.clear();
  }

  base::TimeTicks request_start() const {
    EXPECT_TRUE(peer_context_.received_response);
    return peer_context_.last_load_timing.request_start;
  }
  base::TimeTicks completion_time() const {
    EXPECT_TRUE(peer_context_.complete);
    return peer_context_.completion_status.completion_time;
  }

 private:
  TestRequestPeer::Context peer_context_;
};

TEST_F(CompletionTimeConversionTest, NullCompletionTimestamp) {
  const auto remote_request_start =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(4);

  PerformTest(remote_request_start, base::TimeTicks(), base::TimeDelta());

  EXPECT_EQ(base::TimeTicks(), completion_time());
}

TEST_F(CompletionTimeConversionTest, RemoteRequestStartIsUnavailable) {
  base::TimeTicks begin = base::TimeTicks::Now();

  const auto remote_completion_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(8);

  PerformTest(base::TimeTicks(), remote_completion_time, base::TimeDelta());

  base::TimeTicks end = base::TimeTicks::Now();
  EXPECT_LE(begin, completion_time());
  EXPECT_LE(completion_time(), end);
}

TEST_F(CompletionTimeConversionTest, Convert) {
  const auto remote_request_start =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(4);

  const auto remote_completion_time =
      remote_request_start + base::TimeDelta::FromMilliseconds(3);

  PerformTest(remote_request_start, remote_completion_time,
              base::TimeDelta::FromMilliseconds(15));

  EXPECT_EQ(completion_time(),
            request_start() + base::TimeDelta::FromMilliseconds(3));
}

}  // namespace content
