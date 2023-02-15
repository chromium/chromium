// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_request_peer.h"
#include "third_party/blink/public/platform/web_resource_request_sender_delegate.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "url/gurl.h"

namespace blink {

namespace {

static constexpr char kTestPageUrl[] = "http://www.google.com/";
static constexpr char kTestPageHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static constexpr char kTestPageMimeType[] = "text/html";
static constexpr char kTestPageCharset[] = "";
static constexpr char kTestPageContents[] =
    "<html><head><title>Google</title></head><body><h1>Google</h1></body></"
    "html>";

constexpr size_t kDataPipeCapacity = 4096;

std::string ReadOneChunk(mojo::ScopedDataPipeConsumerHandle* handle) {
  char buffer[kDataPipeCapacity];
  uint32_t read_bytes = kDataPipeCapacity;
  MojoResult result =
      (*handle)->ReadData(buffer, &read_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    return "";
  }
  return std::string(buffer, read_bytes);
}

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

}  // namespace

class TestResourceRequestSenderDelegate
    : public WebResourceRequestSenderDelegate {
 public:
  TestResourceRequestSenderDelegate() = default;
  TestResourceRequestSenderDelegate(const TestResourceRequestSenderDelegate&) =
      delete;
  TestResourceRequestSenderDelegate& operator=(
      const TestResourceRequestSenderDelegate&) = delete;
  ~TestResourceRequestSenderDelegate() override = default;

  void OnRequestComplete() override {}

  scoped_refptr<WebRequestPeer> OnReceivedResponse(
      scoped_refptr<WebRequestPeer> current_peer,
      const WebString& mime_type,
      const WebURL& url) override {
    return base::MakeRefCounted<WrapperPeer>(std::move(current_peer));
  }

  class WrapperPeer : public WebRequestPeer {
   public:
    explicit WrapperPeer(scoped_refptr<WebRequestPeer> original_peer)
        : original_peer_(std::move(original_peer)) {}
    WrapperPeer(const WrapperPeer&) = delete;
    WrapperPeer& operator=(const WrapperPeer&) = delete;

    // WebRequestPeer overrides:
    void OnUploadProgress(uint64_t position, uint64_t size) override {}
    bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                            network::mojom::URLResponseHeadPtr head,
                            std::vector<std::string>*) override {
      return false;
    }
    void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                            base::TimeTicks response_arrival) override {
      response_head_ = std::move(head);
    }
    void OnStartLoadingResponseBody(
        mojo::ScopedDataPipeConsumerHandle body) override {
      body_handle_ = std::move(body);
    }
    void OnTransferSizeUpdated(int transfer_size_diff) override {}
    void OnCompletedRequest(
        const network::URLLoaderCompletionStatus& status) override {
      original_peer_->OnReceivedResponse(std::move(response_head_));
      original_peer_->OnStartLoadingResponseBody(std::move(body_handle_));
      original_peer_->OnCompletedRequest(status);
    }

   private:
    scoped_refptr<WebRequestPeer> original_peer_;
    network::mojom::URLResponseHeadPtr response_head_;
    mojo::ScopedDataPipeConsumerHandle body_handle_;
  };
};

// A mock WebRequestPeer to receive messages from the ResourceRequestSender.
class MockRequestPeer : public WebRequestPeer {
 public:
  explicit MockRequestPeer(ResourceRequestSender* resource_request_sender)
      : resource_request_sender_(resource_request_sender) {}

  // WebRequestPeer overrides:
  void OnUploadProgress(uint64_t position, uint64_t size) override {}
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>* removed_headers) override {
    last_load_timing_ = head->load_timing;
    return true;
  }
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          base::TimeTicks response_arrival) override {
    last_load_timing_ = head->load_timing;
    received_response_ = true;
    if (cancel_on_receive_response_) {
      resource_request_sender_->Cancel(
          scheduler::GetSingleThreadTaskRunnerForTesting());
    }
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    if (cancel_on_receive_response_) {
      return;
    }
    if (body) {
      data_ += ReadOneChunk(&body);
    }
  }
  void OnTransferSizeUpdated(int transfer_size_diff) override {}
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override {}
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    if (cancel_on_receive_response_) {
      return;
    }
    completion_status_ = status;
    complete_ = true;
  }

  std::string data() { return data_; }
  bool received_response() { return received_response_; }
  bool complete() { return complete_; }
  net::LoadTimingInfo last_load_timing() { return last_load_timing_; }
  network::URLLoaderCompletionStatus completion_status() {
    return completion_status_;
  }

  void SetCancelOnReceiveResponse(bool cancel_on_receive_response) {
    cancel_on_receive_response_ = cancel_on_receive_response;
  }

 private:
  // Data received. If downloading to file, remains empty.
  std::string data_;

  bool received_response_ = false;
  bool complete_ = false;
  bool cancel_on_receive_response_ = false;
  net::LoadTimingInfo last_load_timing_;
  network::URLLoaderCompletionStatus completion_status_;
  ResourceRequestSender* resource_request_sender_ = nullptr;
};  // namespace blink

// Sets up the message sender override for the unit test.
class ResourceRequestSenderTest : public testing::Test,
                                  public network::mojom::URLLoaderFactory {
 public:
  explicit ResourceRequestSenderTest()
      : platform_(&delegate_),
        resource_request_sender_(new ResourceRequestSender()) {}

  ~ResourceRequestSenderTest() override {
    resource_request_sender_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& annotation) override {
    loader_and_clients_.emplace_back(std::move(receiver), std::move(client));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }

  void CallOnReceiveResponse(network::mojom::URLLoaderClient* client,
                             mojo::ScopedDataPipeConsumerHandle body) {
    auto head = network::mojom::URLResponseHead::New();
    std::string raw_headers(kTestPageHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head->headers = new net::HttpResponseHeaders(raw_headers);
    head->mime_type = kTestPageMimeType;
    head->charset = kTestPageCharset;
    client->OnReceiveResponse(std::move(head), std::move(body), absl::nullopt);
  }

  std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
    std::unique_ptr<network::ResourceRequest> request(
        new network::ResourceRequest());

    request->method = "GET";
    request->url = GURL(kTestPageUrl);
    request->site_for_cookies =
        net::SiteForCookies::FromUrl(GURL(kTestPageUrl));
    request->referrer_policy = ReferrerUtils::GetDefaultNetReferrerPolicy();
    request->resource_type =
        static_cast<int>(mojom::ResourceType::kSubResource);
    request->priority = net::LOW;
    request->mode = network::mojom::RequestMode::kNoCors;

    auto url_request_extra_data =
        base::MakeRefCounted<WebURLRequestExtraData>();
    url_request_extra_data->CopyToResourceRequest(request.get());

    return request;
  }

  ResourceRequestSender* sender() { return resource_request_sender_.get(); }

  void StartAsync(std::unique_ptr<network::ResourceRequest> request,
                  scoped_refptr<WebRequestPeer> peer) {
    sender()->SendAsync(
        std::move(request), scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        /*cors_exempt_header_list=*/Vector<String>(), std::move(peer),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        /*back_forward_cache_loader_helper=*/nullptr);
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
    explicit TestPlatform(WebResourceRequestSenderDelegate* delegate)
        : delegate_(delegate) {}
    WebResourceRequestSenderDelegate* GetResourceRequestSenderDelegate()
        override {
      return delegate_;
    }

   private:
    WebResourceRequestSenderDelegate* delegate_;
  };

 protected:
  std::vector<std::pair<mojo::PendingReceiver<network::mojom::URLLoader>,
                        mojo::PendingRemote<network::mojom::URLLoaderClient>>>
      loader_and_clients_;
  TestResourceRequestSenderDelegate delegate_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestPlatform, WebResourceRequestSenderDelegate*>
      platform_;
  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  scoped_refptr<MockRequestPeer> mock_peer_;
};

// Tests the generation of unique request ids.
TEST_F(ResourceRequestSenderTest, MakeRequestID) {
  int first_id = GenerateRequestId();
  int second_id = GenerateRequestId();

  // Child process ids are unique (per process) and counting from 0 upwards:
  EXPECT_GT(second_id, first_id);
  EXPECT_GE(first_id, 0);
}

TEST_F(ResourceRequestSenderTest, DelegateTest) {
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  mock_peer_ =
      base::MakeRefCounted<MockRequestPeer>(resource_request_sender_.get());
  StartAsync(std::move(request), mock_peer_);

  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  loader_and_clients_.clear();

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  auto options = DataPipeOptions();
  ASSERT_EQ(mojo::CreateDataPipe(&options, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  // The wrapper eats all messages until RequestComplete message is sent.
  CallOnReceiveResponse(client.get(), std::move(consumer_handle));

  uint32_t size = static_cast<uint32_t>(strlen(kTestPageContents));
  auto result = producer_handle->WriteData(kTestPageContents, &size,
                                           MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(size, strlen(kTestPageContents));

  producer_handle.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(mock_peer_->received_response());

  // This lets the wrapper peer pass all the messages to the original
  // peer at once.
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  status.exists_in_cache = false;
  status.encoded_data_length = strlen(kTestPageContents);
  client->OnComplete(status);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_peer_->received_response());
  EXPECT_EQ(kTestPageContents, mock_peer_->data());
  EXPECT_TRUE(mock_peer_->complete());
}

TEST_F(ResourceRequestSenderTest, CancelDuringCallbackWithWrapperPeer) {
  std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
  mock_peer_ =
      base::MakeRefCounted<MockRequestPeer>(resource_request_sender_.get());
  mock_peer_->SetCancelOnReceiveResponse(true);
  StartAsync(std::move(request), mock_peer_);

  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  loader_and_clients_.clear();

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  auto options = DataPipeOptions();
  ASSERT_EQ(mojo::CreateDataPipe(&options, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  CallOnReceiveResponse(client.get(), std::move(consumer_handle));
  uint32_t size = static_cast<uint32_t>(strlen(kTestPageContents));
  auto result = producer_handle->WriteData(kTestPageContents, &size,
                                           MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(size, strlen(kTestPageContents));
  producer_handle.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(mock_peer_->received_response());

  // This lets the wrapper peer pass all the messages to the original
  // peer at once, but the original peer cancels right after it receives
  // the response. (This will remove pending request info from
  // ResourceRequestSender while the wrapper peer is still running
  // OnCompletedRequest, but it should not lead to crashes.)
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  status.exists_in_cache = false;
  status.encoded_data_length = strlen(kTestPageContents);
  client->OnComplete(status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_peer_->received_response());
  // Request should have been cancelled with no additional messages.
  // EXPECT_TRUE(peer_context.cancelled);
  EXPECT_EQ("", mock_peer_->data());
  EXPECT_FALSE(mock_peer_->complete());
}

class TimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(network::mojom::URLResponseHeadPtr response_head) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    StartAsync(std::move(request), mock_peer_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(loader_and_clients_[0].second));
    loader_and_clients_.clear();
    client->OnReceiveResponse(std::move(response_head),
                              mojo::ScopedDataPipeConsumerHandle(),
                              absl::nullopt);
  }

  const network::mojom::URLResponseHead& response_info() const {
    return *response_info_;
  }

 private:
  network::mojom::URLResponseHeadPtr response_info_ =
      network::mojom::URLResponseHead::New();
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);
  response_head->load_timing.request_start_time = base::Time::Now();
  response_head->load_timing.request_start = TicksFromMicroseconds(10);
  response_head->load_timing.connect_timing.connect_start =
      TicksFromMicroseconds(13);

  auto request_start = response_head->load_timing.request_start;
  PerformTest(std::move(response_head));

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
  EXPECT_LE(request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
}

class CompletionTimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(base::TimeTicks remote_request_start,
                   base::TimeTicks completion_time,
                   base::TimeDelta delay) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    mock_peer_ =
        base::MakeRefCounted<MockRequestPeer>(resource_request_sender_.get());
    StartAsync(std::move(request), mock_peer_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(loader_and_clients_[0].second));
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->request_start = remote_request_start;
    response_head->load_timing.request_start = remote_request_start;
    response_head->load_timing.receive_headers_end = remote_request_start;
    // We need to put something non-null time, otherwise no values will be
    // copied.
    response_head->load_timing.request_start_time =
        base::Time() + base::Seconds(99);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    client->OnReceiveResponse(std::move(response_head),
                              std::move(consumer_handle), absl::nullopt);
    producer_handle.reset();  // The response is empty.

    network::URLLoaderCompletionStatus status;
    status.completion_time = completion_time;

    client->OnComplete(status);

    const base::TimeTicks until = base::TimeTicks::Now() + delay;
    while (base::TimeTicks::Now() < until) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    base::RunLoop().RunUntilIdle();
    loader_and_clients_.clear();
  }

  base::TimeTicks request_start() const {
    EXPECT_TRUE(mock_peer_->received_response());
    return mock_peer_->last_load_timing().request_start;
  }
  base::TimeTicks completion_time() const {
    EXPECT_TRUE(mock_peer_->complete());
    return mock_peer_->completion_status().completion_time;
  }
};

TEST_F(CompletionTimeConversionTest, NullCompletionTimestamp) {
  const auto remote_request_start = base::TimeTicks() + base::Milliseconds(4);

  PerformTest(remote_request_start, base::TimeTicks(), base::TimeDelta());

  EXPECT_EQ(base::TimeTicks(), completion_time());
}

TEST_F(CompletionTimeConversionTest, RemoteRequestStartIsUnavailable) {
  base::TimeTicks begin = base::TimeTicks::Now();

  const auto remote_completion_time = base::TimeTicks() + base::Milliseconds(8);

  PerformTest(base::TimeTicks(), remote_completion_time, base::TimeDelta());

  base::TimeTicks end = base::TimeTicks::Now();
  EXPECT_LE(begin, completion_time());
  EXPECT_LE(completion_time(), end);
}

TEST_F(CompletionTimeConversionTest, Convert) {
  const auto remote_request_start = base::TimeTicks() + base::Milliseconds(4);

  const auto remote_completion_time =
      remote_request_start + base::Milliseconds(3);

  PerformTest(remote_request_start, remote_completion_time,
              base::Milliseconds(15));

  EXPECT_EQ(completion_time(), request_start() + base::Milliseconds(3));
}

}  // namespace blink
