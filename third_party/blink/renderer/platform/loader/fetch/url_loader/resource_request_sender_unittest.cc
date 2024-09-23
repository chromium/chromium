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

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_url_loader_factory_for_background_thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "url/gurl.h"

namespace blink {

namespace {

using RefCountedURLLoaderClientRemote =
    base::RefCountedData<mojo::Remote<network::mojom::URLLoaderClient>>;

static constexpr char kTestPageUrl[] = "http://www.google.com/";
static constexpr char kDifferentUrl[] = "http://www.google.com/different";
static constexpr char kRedirectedUrl[] = "http://redirected.example.com/";
static constexpr char kTestUrlForCodeCacheWithHashing[] =
    "codecachewithhashing://www.example.com/";
static constexpr char kTestData[] = "Hello world";

constexpr size_t kDataPipeCapacity = 4096;

std::string ReadOneChunk(mojo::ScopedDataPipeConsumerHandle* handle) {
  std::string buffer(kDataPipeCapacity, '\0');
  size_t actually_read_bytes = 0;
  MojoResult result = (*handle)->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                          base::as_writable_byte_span(buffer),
                                          actually_read_bytes);
  if (result != MOJO_RESULT_OK) {
    return "";
  }
  return buffer.substr(0, actually_read_bytes);
}

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
  std::unique_ptr<network::ResourceRequest> request(
      new network::ResourceRequest());

  request->method = "GET";
  request->url = GURL(kTestPageUrl);
  request->site_for_cookies = net::SiteForCookies::FromUrl(GURL(kTestPageUrl));
  request->referrer_policy = ReferrerUtils::GetDefaultNetReferrerPolicy();
  request->resource_type = static_cast<int>(mojom::ResourceType::kSubResource);
  request->priority = net::LOW;
  request->mode = network::mojom::RequestMode::kNoCors;

  auto url_request_extra_data = base::MakeRefCounted<WebURLRequestExtraData>();
  url_request_extra_data->CopyToResourceRequest(request.get());

  return request;
}

std::unique_ptr<network::ResourceRequest> CreateSyncResourceRequest() {
  auto request = CreateResourceRequest();
  request->load_flags = net::LOAD_IGNORE_LIMITS;
  return request;
}

mojo::ScopedDataPipeConsumerHandle CreateDataPipeConsumerHandleFilledWithString(
    const std::string& string) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
           MOJO_RESULT_OK);
  CHECK(mojo::BlockingCopyFromString(string, producer_handle));
  return consumer_handle;
}

class TestPlatformForRedirects final : public TestingPlatformSupport {
 public:
  bool IsRedirectSafe(const GURL& from_url, const GURL& to_url) override {
    return true;
  }
};

void RegisterURLSchemeAsCodeCacheWithHashing() {
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");
}

// A mock ResourceRequestClient to receive messages from the
// ResourceRequestSender.
class MockRequestClient : public ResourceRequestClient {
 public:
  MockRequestClient() = default;

  // ResourceRequestClient overrides:
  void OnUploadProgress(uint64_t position, uint64_t size) override {
    upload_progress_called_ = true;
  }
  void OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr head,
      FollowRedirectCallback follow_redirect_callback) override {
    redirected_ = true;
    last_load_timing_ = head->load_timing;
    CHECK(on_received_redirect_callback_);
    std::move(on_received_redirect_callback_)
        .Run(redirect_info, std::move(head),
             std::move(follow_redirect_callback));
  }
  void OnReceivedResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    last_load_timing_ = head->load_timing;
    cached_metadata_ = std::move(cached_metadata);
    received_response_ = true;
    if (body) {
      data_ += ReadOneChunk(&body);
    }
  }
  void OnTransferSizeUpdated(int transfer_size_diff) override {
    transfer_size_updated_called_ = true;
  }
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
    complete_ = true;
  }

  std::string data() { return data_; }
  bool upload_progress_called() const { return upload_progress_called_; }
  bool redirected() const { return redirected_; }
  bool received_response() { return received_response_; }
  const std::optional<mojo_base::BigBuffer>& cached_metadata() const {
    return cached_metadata_;
  }
  bool transfer_size_updated_called() const {
    return transfer_size_updated_called_;
  }
  bool complete() const { return complete_; }
  const net::LoadTimingInfo& last_load_timing() const {
    return last_load_timing_;
  }
  network::URLLoaderCompletionStatus completion_status() {
    return completion_status_;
  }

  void SetOnReceivedRedirectCallback(
      base::OnceCallback<void(const net::RedirectInfo&,
                              network::mojom::URLResponseHeadPtr,
                              FollowRedirectCallback)> callback) {
    on_received_redirect_callback_ = std::move(callback);
  }

 private:
  // Data received. If downloading to file, remains empty.
  std::string data_;

  bool upload_progress_called_ = false;
  bool redirected_ = false;
  bool transfer_size_updated_called_ = false;
  bool received_response_ = false;
  std::optional<mojo_base::BigBuffer> cached_metadata_;
  bool complete_ = false;
  net::LoadTimingInfo last_load_timing_;
  network::URLLoaderCompletionStatus completion_status_;
  base::OnceCallback<void(const net::RedirectInfo&,
                          network::mojom::URLResponseHeadPtr,
                          FollowRedirectCallback)>
      on_received_redirect_callback_;
};

class MockLoader : public network::mojom::URLLoader {
 public:
  using RepeatingFollowRedirectCallback = base::RepeatingCallback<void(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers)>;
  MockLoader() = default;
  MockLoader(const MockLoader&) = delete;
  MockLoader& operator=(const MockLoader&) = delete;
  ~MockLoader() override = default;

  // network::mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    if (follow_redirect_callback_) {
      follow_redirect_callback_.Run(removed_headers, modified_headers);
    }
  }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void SetFollowRedirectCallback(RepeatingFollowRedirectCallback callback) {
    follow_redirect_callback_ = std::move(callback);
  }

 private:
  RepeatingFollowRedirectCallback follow_redirect_callback_;
};

using FetchCachedCodeCallback =
    mojom::blink::CodeCacheHost::FetchCachedCodeCallback;
using ProcessCodeCacheRequestCallback = base::RepeatingCallback<
    void(mojom::blink::CodeCacheType, const KURL&, FetchCachedCodeCallback)>;

class DummyCodeCacheHost final : public mojom::blink::CodeCacheHost {
 public:
  explicit DummyCodeCacheHost(
      ProcessCodeCacheRequestCallback process_code_cache_request_callback)
      : process_code_cache_request_callback_(
            std::move(process_code_cache_request_callback)) {
    mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote;
    receiver_ = std::make_unique<mojo::Receiver<mojom::blink::CodeCacheHost>>(
        this, pending_remote.InitWithNewPipeAndPassReceiver());
    host_ = std::make_unique<blink::CodeCacheHost>(
        mojo::Remote<mojom::blink::CodeCacheHost>(std::move(pending_remote)));
  }

  // mojom::blink::CodeCacheHost implementations
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {}
  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback callback) override {
    process_code_cache_request_callback_.Run(cache_type, url,
                                             std::move(callback));
  }
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const KURL& url) override {
    did_clear_code_cache_entry_ = true;
  }
  void DidGenerateCacheableMetadataInCacheStorage(
      const KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const WTF::String& cache_storage_cache_name) override {}

  blink::CodeCacheHost* GetCodeCacheHost() { return host_.get(); }
  bool did_clear_code_cache_entry() const {
    return did_clear_code_cache_entry_;
  }

 private:
  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
  std::unique_ptr<mojo::Receiver<mojom::blink::CodeCacheHost>> receiver_;
  std::unique_ptr<blink::CodeCacheHost> host_;
  bool did_clear_code_cache_entry_ = false;
};

// Sets up the message sender override for the unit test.
class ResourceRequestSenderTest : public testing::Test,
                                  public network::mojom::URLLoaderFactory {
 public:
  explicit ResourceRequestSenderTest()
      : resource_request_sender_(new ResourceRequestSender()) {}

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
    NOTREACHED_IN_MIGRATION();
  }

 protected:
  ResourceRequestSender* sender() { return resource_request_sender_.get(); }

  void StartAsync(std::unique_ptr<network::ResourceRequest> request,
                  scoped_refptr<ResourceRequestClient> client,
                  CodeCacheHost* code_cache_host = nullptr) {
    sender()->SendAsync(
        std::move(request), scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        /*cors_exempt_header_list=*/Vector<String>(), std::move(client),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        code_cache_host,
        /*evict_from_bfcache_callback=*/
        base::OnceCallback<void(mojom::blink::RendererEvictionReason)>(),
        /*did_buffer_load_while_in_bfcache_callback=*/
        base::RepeatingCallback<void(size_t)>());
  }

  network::mojom::URLResponseHeadPtr CreateResponse() {
    auto response = network::mojom::URLResponseHead::New();
    response->response_time = base::Time::Now();
    return response;
  }

  std::vector<std::pair<mojo::PendingReceiver<network::mojom::URLLoader>,
                        mojo::PendingRemote<network::mojom::URLLoaderClient>>>
      loader_and_clients_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  scoped_refptr<MockRequestClient> mock_client_;

 private:
  ScopedTestingPlatformSupport<TestPlatformForRedirects> platform_;
};

// Tests the generation of unique request ids.
TEST_F(ResourceRequestSenderTest, MakeRequestID) {
  int first_id = GenerateRequestId();
  int second_id = GenerateRequestId();

  // Child process ids are unique (per process) and counting from 0 upwards:
  EXPECT_GT(second_id, first_id);
  EXPECT_GE(first_id, 0);
}

TEST_F(ResourceRequestSenderTest, RedirectSyncFollow) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with an
        // empty `removed_headers` and empty `modified_headers`.
        EXPECT_TRUE(removed_headers.empty());
        EXPECT_TRUE(modified_headers.IsEmpty());
        run_loop_for_redirect.Quit();
      }));

  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Synchronously call `callback` with an empty `removed_headers` and
        // empty `modified_headers`.
        std::move(callback).Run({}, {});
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectSyncFollowWithRemovedHeaders) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with a
        // non-empty `removed_headers` and empty `modified_headers.
        EXPECT_THAT(removed_headers,
                    ::testing::ElementsAreArray({"Foo-Bar", "Hoge-Piyo"}));
        EXPECT_TRUE(modified_headers.IsEmpty());
        run_loop_for_redirect.Quit();
      }));

  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Synchronously call `callback` with a non-empty `removed_headers` and
        // empty `modified_headers`.
        std::move(callback).Run({"Foo-Bar", "Hoge-Piyo"}, {});
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectSyncFollowWithModifiedHeaders) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with an
        // empty `removed_headers` and non-empty `modified_headers.
        EXPECT_TRUE(removed_headers.empty());
        EXPECT_EQ(
            "Cookie-Monster: Nom nom nom\r\nDomo-Kun: Loves Chrome\r\n\r\n",
            modified_headers.ToString());
        run_loop_for_redirect.Quit();
      }));

  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Synchronously call `callback` with an empty `removed_headers` and
        // non-empty `modified_headers`.
        net::HttpRequestHeaders modified_headers;
        modified_headers.SetHeader("Cookie-Monster", "Nom nom nom");
        modified_headers.SetHeader("Domo-Kun", "Loves Chrome");
        std::move(callback).Run({}, std::move(modified_headers));
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectSyncCancel) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  mock_loader_prt->SetFollowRedirectCallback(
      base::BindRepeating([](const std::vector<std::string>& removed_headers,
                             const net::HttpRequestHeaders& modified_headers) {
        // FollowRedirect() must not be called.
        CHECK(false);
      }));

  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Synchronously cancels the request in the `OnReceivedRedirect()`.
        sender()->Cancel(scheduler::GetSingleThreadTaskRunnerForTesting());
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ResourceRequestSenderTest, RedirectAsyncFollow) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with an
        // empty `removed_headers` and empty `modified_headers.
        EXPECT_TRUE(removed_headers.empty());
        EXPECT_TRUE(modified_headers.IsEmpty());
        run_loop_for_redirect.Quit();
      }));

  std::optional<net::RedirectInfo> received_redirect_info;
  ResourceRequestClient::FollowRedirectCallback follow_redirect_callback;
  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        received_redirect_info = redirect_info;
        follow_redirect_callback = std::move(callback);
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(received_redirect_info);
  EXPECT_EQ(GURL(kRedirectedUrl), received_redirect_info->new_url);
  // Asynchronously call `callback` with an empty `removed_headers` and empty
  // `modified_headers`.
  std::move(follow_redirect_callback).Run({}, {});
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectAsyncFollowWithRemovedHeaders) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with a
        // non-empty `removed_headers` and empty `modified_headers.
        EXPECT_THAT(removed_headers,
                    ::testing::ElementsAreArray({"Foo-Bar", "Hoge-Piyo"}));
        EXPECT_TRUE(modified_headers.IsEmpty());
        run_loop_for_redirect.Quit();
      }));

  std::optional<net::RedirectInfo> received_redirect_info;
  ResourceRequestClient::FollowRedirectCallback follow_redirect_callback;
  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        received_redirect_info = redirect_info;
        follow_redirect_callback = std::move(callback);
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(received_redirect_info);
  EXPECT_EQ(GURL(kRedirectedUrl), received_redirect_info->new_url);

  // Asynchronously call `callback` with a non-empty `removed_headers` and an
  // empty `modified_headers`.
  std::move(follow_redirect_callback).Run({"Foo-Bar", "Hoge-Piyo"}, {});
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectAsyncFollowWithModifiedHeaders) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  base::RunLoop run_loop_for_redirect;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        // network::mojom::URLLoader::FollowRedirect() must be called with an
        // empty `removed_headers` and non-empty `modified_headers.
        EXPECT_TRUE(removed_headers.empty());
        EXPECT_EQ(
            "Cookie-Monster: Nom nom nom\r\nDomo-Kun: Loves Chrome\r\n\r\n",
            modified_headers.ToString());
        run_loop_for_redirect.Quit();
      }));

  std::optional<net::RedirectInfo> received_redirect_info;
  ResourceRequestClient::FollowRedirectCallback follow_redirect_callback;
  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        received_redirect_info = redirect_info;
        follow_redirect_callback = std::move(callback);
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(received_redirect_info);
  EXPECT_EQ(GURL(kRedirectedUrl), received_redirect_info->new_url);

  // Asynchronously call `callback` with an empty `removed_headers` and
  // non-empty `modified_headers`.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader("Cookie-Monster", "Nom nom nom");
  modified_headers.SetHeader("Domo-Kun", "Loves Chrome");
  std::move(follow_redirect_callback).Run({}, std::move(modified_headers));
  run_loop_for_redirect.Run();
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

TEST_F(ResourceRequestSenderTest, RedirectAsyncFollowAfterCancel) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();
  StartAsync(CreateResourceRequest(), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  mock_loader_prt->SetFollowRedirectCallback(
      base::BindRepeating([](const std::vector<std::string>& removed_headers,
                             const net::HttpRequestHeaders& modified_headers) {
        // FollowRedirect() must not be called.
        CHECK(false);
      }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);

  std::optional<net::RedirectInfo> received_redirect_info;
  ResourceRequestClient::FollowRedirectCallback follow_redirect_callback;
  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        received_redirect_info = redirect_info;
        follow_redirect_callback = std::move(callback);
      }));
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(received_redirect_info);
  EXPECT_EQ(redirect_info.new_url, received_redirect_info->new_url);

  // Aynchronously cancels the request.
  sender()->Cancel(scheduler::GetSingleThreadTaskRunnerForTesting());
  std::move(follow_redirect_callback).Run({}, {});
  base::RunLoop().RunUntilIdle();
}

TEST_F(ResourceRequestSenderTest, ReceiveResponseWithoutMetadata) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  StartAsync(std::move(request), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  // Send a response without metadata.
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
}

TEST_F(ResourceRequestSenderTest, ReceiveResponseWithMetadata) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  StartAsync(std::move(request), mock_client_);
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  // Send a response with metadata.
  std::vector<uint8_t> metadata{1, 2, 3, 4, 5};
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo_base::BigBuffer(metadata));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(metadata.size(), mock_client_->cached_metadata()->size());
}

TEST_F(ResourceRequestSenderTest, EmptyCodeCacheThenReceiveResponse) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            EXPECT_EQ(mojom::blink::CodeCacheType::kJavascript, cache_type);
            EXPECT_EQ(kTestPageUrl, url);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();
  // Send an empty cached data from CodeCacheHost.
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer());
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, ReceiveCodeCacheThenReceiveResponse) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(cache_data.size(), mock_client_->cached_metadata()->size());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveTimeMismatchCodeCacheThenReceiveResponse) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time - base::Seconds(1),
           mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveEmptyCodeCacheThenReceiveResponseWithMetadata) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  // Send an empty cached data from CodeCacheHost.
  run_loop.Run();
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer());
  base::RunLoop().RunUntilIdle();

  // Send a response with metadata.
  std::vector<uint8_t> metadata{1, 2, 3, 4, 5};
  client->OnReceiveResponse(CreateResponse(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo_base::BigBuffer(metadata));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(metadata.size(), mock_client_->cached_metadata()->size());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveCodeCacheThenReceiveResponseWithMetadata) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response with metadata.
  std::vector<uint8_t> metadata{1, 2, 3, 4, 5};
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo_base::BigBuffer(metadata));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(metadata.size(), mock_client_->cached_metadata()->size());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveResponseWithMetadataThenReceiveCodeCache) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  auto response_time = response->response_time;

  // Send a response with metadata.
  std::vector<uint8_t> metadata{1, 2, 3, 4, 5};
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo_base::BigBuffer(metadata));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(metadata.size(), mock_client_->cached_metadata()->size());

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response_time, mojo_base::BigBuffer(cache_data));

  base::RunLoop().RunUntilIdle();
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveResponseWithMetadataThenReceiveEmptyCodeCache) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();

  // Send a response with metadata.
  std::vector<uint8_t> metadata{1, 2, 3, 4, 5};
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo_base::BigBuffer(metadata));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(metadata.size(), mock_client_->cached_metadata()->size());

  // Send an empty cached data from CodeCacheHost.
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer());

  base::RunLoop().RunUntilIdle();
  // Code cache must be cleared.
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, SlowCodeCache) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));
  std::unique_ptr<MockLoader> mock_loader = std::make_unique<MockLoader>();
  MockLoader* mock_loader_prt = mock_loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                              std::move(loader_and_clients_[0].first));

  run_loop.Run();

  bool follow_redirect_callback_called = false;
  mock_loader_prt->SetFollowRedirectCallback(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& removed_headers,
          const net::HttpRequestHeaders& modified_headers) {
        follow_redirect_callback_called = true;
      }));

  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        std::move(callback).Run({}, {});
      }));

  auto response = CreateResponse();
  auto response_time = response->response_time;

  // Call URLLoaderClient IPCs.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kRedirectedUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  client->OnUploadProgress(/*current_position=*/10, /*total_size=*/10,
                           base::BindLambdaForTesting([]() {}));
  client->OnReceiveResponse(
      std::move(response),
      CreateDataPipeConsumerHandleFilledWithString(kTestData), std::nullopt);
  client->OnTransferSizeUpdated(100);
  client->OnComplete(network::URLLoaderCompletionStatus(net::Error::OK));
  base::RunLoop().RunUntilIdle();

  // MockRequestClient should not have received any response.
  EXPECT_FALSE(mock_client_->redirected());
  EXPECT_FALSE(follow_redirect_callback_called);
  EXPECT_FALSE(mock_client_->upload_progress_called());
  EXPECT_FALSE(mock_client_->received_response());
  EXPECT_TRUE(mock_client_->data().empty());
  EXPECT_FALSE(mock_client_->transfer_size_updated_called());
  EXPECT_FALSE(mock_client_->complete());

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response_time, mojo_base::BigBuffer(cache_data));

  base::RunLoop().RunUntilIdle();

  // MockRequestClient must have received the response.
  EXPECT_TRUE(mock_client_->redirected());
  EXPECT_TRUE(follow_redirect_callback_called);
  EXPECT_TRUE(mock_client_->upload_progress_called());
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_EQ(kTestData, mock_client_->data());
  EXPECT_TRUE(mock_client_->transfer_size_updated_called());
  EXPECT_TRUE(mock_client_->complete());

  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(cache_data.size(), mock_client_->cached_metadata()->size());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, ReceiveCodeCacheWhileFrozen) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  auto response_time = response->response_time;

  // Call URLLoaderClient IPCs.
  client->OnUploadProgress(/*current_position=*/10, /*total_size=*/10,
                           base::BindLambdaForTesting([]() {}));
  client->OnReceiveResponse(
      std::move(response),
      CreateDataPipeConsumerHandleFilledWithString(kTestData), std::nullopt);
  client->OnTransferSizeUpdated(100);
  client->OnComplete(network::URLLoaderCompletionStatus(net::Error::OK));
  base::RunLoop().RunUntilIdle();

  // MockRequestClient should not have received any response.
  EXPECT_FALSE(mock_client_->upload_progress_called());
  EXPECT_FALSE(mock_client_->received_response());
  EXPECT_TRUE(mock_client_->data().empty());
  EXPECT_FALSE(mock_client_->transfer_size_updated_called());
  EXPECT_FALSE(mock_client_->complete());

  // Freeze the sender.
  sender()->Freeze(LoaderFreezeMode::kStrict);

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response_time, mojo_base::BigBuffer(cache_data));

  base::RunLoop().RunUntilIdle();

  // MockRequestClient should not have received any response.
  EXPECT_FALSE(mock_client_->upload_progress_called());
  EXPECT_FALSE(mock_client_->received_response());
  EXPECT_TRUE(mock_client_->data().empty());
  EXPECT_FALSE(mock_client_->transfer_size_updated_called());
  EXPECT_FALSE(mock_client_->complete());

  // Unfreeze the sender.
  sender()->Freeze(LoaderFreezeMode::kNone);

  base::RunLoop().RunUntilIdle();

  // MockRequestClient must have received the response.
  EXPECT_TRUE(mock_client_->upload_progress_called());
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_EQ(kTestData, mock_client_->data());
  EXPECT_TRUE(mock_client_->transfer_size_updated_called());
  EXPECT_TRUE(mock_client_->complete());

  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(cache_data.size(), mock_client_->cached_metadata()->size());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveCodeCacheThenReceiveSyntheticResponseFromServiceWorker) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  response->was_fetched_via_service_worker = true;

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveCodeCacheThenReceivePassThroughResponseFromServiceWorker) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  response->was_fetched_via_service_worker = true;
  response->url_list_via_service_worker.emplace_back(kTestPageUrl);

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(cache_data.size(), mock_client_->cached_metadata()->size());
  // Code cache must not be cleared.
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveCodeCacheThenReceiveDifferentUrlResponseFromServiceWorker) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  response->was_fetched_via_service_worker = true;
  response->url_list_via_service_worker.emplace_back(kDifferentUrl);

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       ReceiveCodeCacheThenReceiveResponseFromCacheStorageViaServiceWorker) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  auto response = CreateResponse();
  response->was_fetched_via_service_worker = true;
  response->cache_storage_cache_name = "dummy";

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(response->response_time, mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(std::move(response),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, CodeCacheWithHashingEmptyCodeCache) {
  RegisterURLSchemeAsCodeCacheWithHashing();
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->url = GURL(kTestUrlForCodeCacheWithHashing);
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  // Send an empty cached data from CodeCacheHost.
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer());
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(CreateResponse(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(0u, mock_client_->cached_metadata()->size());
  // Code cache must not be cleared.
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, CodeCacheWithHashingWithCodeCache) {
  RegisterURLSchemeAsCodeCacheWithHashing();
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->url = GURL(kTestUrlForCodeCacheWithHashing);
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Send a response without metadata.
  client->OnReceiveResponse(CreateResponse(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  ASSERT_TRUE(mock_client_->cached_metadata());
  EXPECT_EQ(cache_data.size(), mock_client_->cached_metadata()->size());
  // Code cache must not be cleared.
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest,
       CodeCacheWithHashingWithCodeCacheAfterRedirectedToDifferentScheme) {
  RegisterURLSchemeAsCodeCacheWithHashing();
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->url = GURL(kTestUrlForCodeCacheWithHashing);
  request->destination = network::mojom::RequestDestination::kScript;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();

  // Send a cached data from CodeCacheHost.
  std::vector<uint8_t> cache_data{1, 2, 3, 4, 5, 6};
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer(cache_data));
  base::RunLoop().RunUntilIdle();

  // Redirect to different scheme URL.
  mock_client_->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kTestPageUrl), redirect_info.new_url);
        // Synchronously call `callback` with an empty `removed_headers`.
        std::move(callback).Run({}, {});
      }));
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kTestPageUrl);
  client->OnReceiveRedirect(redirect_info,
                            network::mojom::URLResponseHead::New());
  base::RunLoop().RunUntilIdle();

  // Different scheme redirect triggers another CreateLoaderAndStart() call.
  ASSERT_EQ(2u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> second_client(
      std::move(loader_and_clients_[1].second));

  // Send a response without metadata.
  second_client->OnReceiveResponse(
      CreateResponse(), mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  // Code cache must be cleared.
  EXPECT_TRUE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, WebAssemblyCodeCacheRequest) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->destination = network::mojom::RequestDestination::kEmpty;

  base::RunLoop run_loop;
  FetchCachedCodeCallback fetch_cached_code_callback;
  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            fetch_cached_code_callback = std::move(callback);
            // When `destination` is RequestDestination::kEmpty, `cache_type`
            // must be `CodeCacheType::kWebAssembly`.
            EXPECT_EQ(mojom::blink::CodeCacheType::kWebAssembly, cache_type);
            EXPECT_EQ(kTestPageUrl, url);
            run_loop.Quit();
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  run_loop.Run();
  std::move(fetch_cached_code_callback)
      .Run(base::Time(), mojo_base::BigBuffer());
  base::RunLoop().RunUntilIdle();

  client->OnReceiveResponse(CreateResponse(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
  EXPECT_FALSE(mock_client_->cached_metadata());
  EXPECT_FALSE(code_cache_host->did_clear_code_cache_entry());
}

TEST_F(ResourceRequestSenderTest, KeepaliveRequest) {
  mock_client_ = base::MakeRefCounted<MockRequestClient>();

  std::unique_ptr<network::ResourceRequest> request = CreateResourceRequest();
  request->keepalive = true;

  auto code_cache_host =
      std::make_unique<DummyCodeCacheHost>(base::BindLambdaForTesting(
          [&](mojom::blink::CodeCacheType cache_type, const KURL& url,
              FetchCachedCodeCallback callback) {
            CHECK(false) << "FetchCachedCode shouold not be called";
          }));

  StartAsync(std::move(request), mock_client_,
             code_cache_host->GetCodeCacheHost());
  ASSERT_EQ(1u, loader_and_clients_.size());
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(loader_and_clients_[0].second));

  client->OnReceiveResponse(CreateResponse(),
                            mojo::ScopedDataPipeConsumerHandle(), std::nullopt);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_client_->received_response());
}

class ResourceRequestSenderSyncTest : public testing::Test {
 public:
  explicit ResourceRequestSenderSyncTest() = default;
  ResourceRequestSenderSyncTest(const ResourceRequestSenderSyncTest&) = delete;
  ResourceRequestSenderSyncTest& operator=(
      const ResourceRequestSenderSyncTest&) = delete;
  ~ResourceRequestSenderSyncTest() override = default;

 protected:
  SyncLoadResponse SendSync(
      scoped_refptr<ResourceRequestClient> client,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
    base::WaitableEvent terminate_sync_load_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    SyncLoadResponse response;
    auto sender = std::make_unique<ResourceRequestSender>();
    sender->SendSync(CreateSyncResourceRequest(), TRAFFIC_ANNOTATION_FOR_TESTS,
                     network::mojom::kURLLoadOptionSynchronous, &response,
                     std::move(loader_factory),
                     /*throttles=*/{},
                     /*timeout=*/base::Seconds(100),
                     /*cors_exempt_header_list*/ {}, &terminate_sync_load_event,
                     /*download_to_blob_registry=*/
                     mojo::PendingRemote<mojom::blink::BlobRegistry>(), client,
                     std::make_unique<ResourceLoadInfoNotifierWrapper>(
                         /*resource_load_info_notifier=*/nullptr));
    return response;
  }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  ScopedTestingPlatformSupport<TestPlatformForRedirects> platform_;
};

TEST_F(ResourceRequestSenderSyncTest, SendSyncRequest) {
  scoped_refptr<MockRequestClient> mock_client =
      base::MakeRefCounted<MockRequestClient>();
  auto loader_factory =
      base::MakeRefCounted<
          FakeURLLoaderFactoryForBackgroundThread>(base::BindOnce(
          [](mojo::PendingReceiver<network::mojom::URLLoader> loader,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
            mojo::MakeSelfOwnedReceiver(std::make_unique<MockLoader>(),
                                        std::move(loader));
            mojo::Remote<network::mojom::URLLoaderClient> loader_client(
                std::move(client));
            loader_client->OnReceiveResponse(
                network::mojom::URLResponseHead::New(),
                CreateDataPipeConsumerHandleFilledWithString(kTestData),
                std::nullopt);
            loader_client->OnComplete(
                network::URLLoaderCompletionStatus(net::Error::OK));
          }));
  SyncLoadResponse response = SendSync(mock_client, std::move(loader_factory));
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(kTestData,
            std::string(response.data->begin()->data(), response.data->size()));
}

TEST_F(ResourceRequestSenderSyncTest, SendSyncRedirect) {
  scoped_refptr<MockRequestClient> mock_client =
      base::MakeRefCounted<MockRequestClient>();
  mock_client->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Synchronously call `callback` with an empty `removed_headers` and
        // empty `modified_headers`.
        std::move(callback).Run({}, {});
      }));

  auto loader_factory = base::MakeRefCounted<
      FakeURLLoaderFactoryForBackgroundThread>(base::BindOnce(
      [](mojo::PendingReceiver<network::mojom::URLLoader> loader,
         mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
        std::unique_ptr<MockLoader> mock_loader =
            std::make_unique<MockLoader>();
        MockLoader* mock_loader_prt = mock_loader.get();
        mojo::MakeSelfOwnedReceiver(std::move(mock_loader), std::move(loader));

        mojo::Remote<network::mojom::URLLoaderClient> loader_client(
            std::move(client));

        net::RedirectInfo redirect_info;
        redirect_info.new_url = GURL(kRedirectedUrl);
        loader_client->OnReceiveRedirect(
            redirect_info, network::mojom::URLResponseHead::New());

        scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client =
            base::MakeRefCounted<RefCountedURLLoaderClientRemote>(
                std::move(loader_client));

        mock_loader_prt->SetFollowRedirectCallback(base::BindRepeating(
            [](scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client,
               const std::vector<std::string>& removed_headers,
               const net::HttpRequestHeaders& modified_headers) {
              // network::mojom::URLLoader::FollowRedirect() must be called with
              // an empty `removed_headers` and empty `modified_headers.
              EXPECT_TRUE(removed_headers.empty());
              EXPECT_TRUE(modified_headers.IsEmpty());

              // After FollowRedirect() is called, calls
              // URLLoaderClient::OnReceiveResponse() and
              // URLLoaderClient::OnComplete()
              refcounted_client->data->OnReceiveResponse(
                  network::mojom::URLResponseHead::New(),
                  CreateDataPipeConsumerHandleFilledWithString(kTestData),
                  std::nullopt);

              refcounted_client->data->OnComplete(
                  network::URLLoaderCompletionStatus(net::Error::OK));
            },
            std::move(refcounted_client)));
      }));

  SyncLoadResponse response = SendSync(mock_client, std::move(loader_factory));
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(kTestData,
            std::string(response.data->begin()->data(), response.data->size()));
}

TEST_F(ResourceRequestSenderSyncTest, SendSyncRedirectWithRemovedHeaders) {
  scoped_refptr<MockRequestClient> mock_client =
      base::MakeRefCounted<MockRequestClient>();
  mock_client->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // network::mojom::URLLoader::FollowRedirect() must be called with a
        // non-empty `removed_headers` and empty `modified_headers.
        std::move(callback).Run({"Foo-Bar", "Hoge-Piyo"}, {});
      }));

  auto loader_factory = base::MakeRefCounted<
      FakeURLLoaderFactoryForBackgroundThread>(base::BindOnce(
      [](mojo::PendingReceiver<network::mojom::URLLoader> loader,
         mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
        std::unique_ptr<MockLoader> mock_loader =
            std::make_unique<MockLoader>();
        MockLoader* mock_loader_prt = mock_loader.get();
        mojo::MakeSelfOwnedReceiver(std::move(mock_loader), std::move(loader));

        mojo::Remote<network::mojom::URLLoaderClient> loader_client(
            std::move(client));

        net::RedirectInfo redirect_info;
        redirect_info.new_url = GURL(kRedirectedUrl);
        loader_client->OnReceiveRedirect(
            redirect_info, network::mojom::URLResponseHead::New());

        scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client =
            base::MakeRefCounted<RefCountedURLLoaderClientRemote>(
                std::move(loader_client));

        mock_loader_prt->SetFollowRedirectCallback(base::BindRepeating(
            [](scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client,
               const std::vector<std::string>& removed_headers,
               const net::HttpRequestHeaders& modified_headers) {
              // Synchronously call `callback` with a non-empty
              // `removed_headers` and empty `modified_headers.
              EXPECT_THAT(removed_headers, ::testing::ElementsAreArray(
                                               {"Foo-Bar", "Hoge-Piyo"}));
              EXPECT_TRUE(modified_headers.IsEmpty());

              // After FollowRedirect() is called, calls
              // URLLoaderClient::OnReceiveResponse() and
              // URLLoaderClient::OnComplete()
              refcounted_client->data->OnReceiveResponse(
                  network::mojom::URLResponseHead::New(),
                  CreateDataPipeConsumerHandleFilledWithString(kTestData),
                  std::nullopt);
              refcounted_client->data->OnComplete(
                  network::URLLoaderCompletionStatus(net::Error::OK));
            },
            std::move(refcounted_client)));
      }));

  SyncLoadResponse response = SendSync(mock_client, std::move(loader_factory));
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(kTestData,
            std::string(response.data->begin()->data(), response.data->size()));
}

TEST_F(ResourceRequestSenderSyncTest, SendSyncRedirectWithModifiedHeaders) {
  scoped_refptr<MockRequestClient> mock_client =
      base::MakeRefCounted<MockRequestClient>();
  mock_client->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // network::mojom::URLLoader::FollowRedirect() must be called with an
        // empty `removed_headers` and non-empty `modified_headers.
        net::HttpRequestHeaders modified_headers;
        modified_headers.SetHeader("Cookie-Monster", "Nom nom nom");
        modified_headers.SetHeader("Domo-Kun", "Loves Chrome");
        std::move(callback).Run({}, std::move(modified_headers));
      }));

  auto loader_factory = base::MakeRefCounted<
      FakeURLLoaderFactoryForBackgroundThread>(base::BindOnce(
      [](mojo::PendingReceiver<network::mojom::URLLoader> loader,
         mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
        std::unique_ptr<MockLoader> mock_loader =
            std::make_unique<MockLoader>();
        MockLoader* mock_loader_prt = mock_loader.get();
        mojo::MakeSelfOwnedReceiver(std::move(mock_loader), std::move(loader));

        mojo::Remote<network::mojom::URLLoaderClient> loader_client(
            std::move(client));

        net::RedirectInfo redirect_info;
        redirect_info.new_url = GURL(kRedirectedUrl);
        loader_client->OnReceiveRedirect(
            redirect_info, network::mojom::URLResponseHead::New());

        scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client =
            base::MakeRefCounted<RefCountedURLLoaderClientRemote>(
                std::move(loader_client));

        mock_loader_prt->SetFollowRedirectCallback(base::BindRepeating(
            [](scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client,
               const std::vector<std::string>& removed_headers,
               const net::HttpRequestHeaders& modified_headers) {
              // Synchronously call `callback` with an empty
              // `removed_headers` and non-empty `modified_headers.
              EXPECT_TRUE(removed_headers.empty());
              EXPECT_EQ(
                  "Cookie-Monster: Nom nom nom\r\nDomo-Kun: Loves "
                  "Chrome\r\n\r\n",
                  modified_headers.ToString());

              // After FollowRedirect() is called, calls
              // URLLoaderClient::OnReceiveResponse() and
              // URLLoaderClient::OnComplete()
              refcounted_client->data->OnReceiveResponse(
                  network::mojom::URLResponseHead::New(),
                  CreateDataPipeConsumerHandleFilledWithString(kTestData),
                  std::nullopt);
              refcounted_client->data->OnComplete(
                  network::URLLoaderCompletionStatus(net::Error::OK));
            },
            std::move(refcounted_client)));
      }));

  SyncLoadResponse response = SendSync(mock_client, std::move(loader_factory));
  EXPECT_EQ(net::OK, response.error_code);
  ASSERT_TRUE(response.data);
  EXPECT_EQ(kTestData,
            std::string(response.data->begin()->data(), response.data->size()));
}

TEST_F(ResourceRequestSenderSyncTest, SendSyncRedirectCancel) {
  scoped_refptr<MockRequestClient> mock_client =
      base::MakeRefCounted<MockRequestClient>();
  mock_client->SetOnReceivedRedirectCallback(base::BindLambdaForTesting(
      [&](const net::RedirectInfo& redirect_info,
          network::mojom::URLResponseHeadPtr head,
          ResourceRequestClient::FollowRedirectCallback callback) {
        EXPECT_EQ(GURL(kRedirectedUrl), redirect_info.new_url);
        // Don't call callback to cancel the request.
      }));

  auto loader_factory =
      base::MakeRefCounted<
          FakeURLLoaderFactoryForBackgroundThread>(base::BindOnce(
          [](mojo::PendingReceiver<network::mojom::URLLoader> loader,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
            std::unique_ptr<MockLoader> mock_loader =
                std::make_unique<MockLoader>();
            MockLoader* mock_loader_prt = mock_loader.get();
            mojo::MakeSelfOwnedReceiver(std::move(mock_loader),
                                        std::move(loader));

            mojo::Remote<network::mojom::URLLoaderClient> loader_client(
                std::move(client));

            net::RedirectInfo redirect_info;
            redirect_info.new_url = GURL(kRedirectedUrl);
            loader_client->OnReceiveRedirect(
                redirect_info, network::mojom::URLResponseHead::New());

            scoped_refptr<RefCountedURLLoaderClientRemote> refcounted_client =
                base::MakeRefCounted<RefCountedURLLoaderClientRemote>(
                    std::move(loader_client));

            mock_loader_prt->SetFollowRedirectCallback(base::BindRepeating(
                [](scoped_refptr<base::RefCountedData<mojo::Remote<
                       network::mojom::URLLoaderClient>>> refcounted_client,
                   const std::vector<std::string>& removed_headers,
                   const net::HttpRequestHeaders& modified_headers) {
                  // FollowRedirect() must not be called.
                  CHECK(false);
                },
                std::move(refcounted_client)));
          }));

  SyncLoadResponse response = SendSync(mock_client, std::move(loader_factory));
  EXPECT_EQ(net::ERR_ABORTED, response.error_code);
  EXPECT_FALSE(response.data);
}

class TimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(network::mojom::URLResponseHeadPtr response_head) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    mock_client_ = base::MakeRefCounted<MockRequestClient>();
    StartAsync(std::move(request), mock_client_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(loader_and_clients_[0].second));
    loader_and_clients_.clear();
    client->OnReceiveResponse(std::move(response_head),
                              mojo::ScopedDataPipeConsumerHandle(),
                              std::nullopt);
    base::RunLoop().RunUntilIdle();
  }
  const net::LoadTimingInfo& received_load_timing() const {
    CHECK(mock_client_);
    return mock_client_->last_load_timing();
  }
};

TEST_F(TimeConversionTest, ProperlyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);
  response_head->load_timing.request_start_time = base::Time::Now();
  response_head->load_timing.request_start = TicksFromMicroseconds(10);
  response_head->load_timing.connect_timing.connect_start =
      TicksFromMicroseconds(13);

  auto request_start = response_head->load_timing.request_start;
  PerformTest(std::move(response_head));

  EXPECT_LT(base::TimeTicks(), received_load_timing().request_start);
  EXPECT_EQ(base::TimeTicks(),
            received_load_timing().connect_timing.domain_lookup_start);
  EXPECT_LE(request_start, received_load_timing().connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), received_load_timing().request_start);
  EXPECT_EQ(base::TimeTicks(),
            received_load_timing().connect_timing.domain_lookup_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), received_load_timing().request_start);
  EXPECT_EQ(base::TimeTicks(),
            received_load_timing().connect_timing.domain_lookup_start);
}

class CompletionTimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(base::TimeTicks remote_request_start,
                   base::TimeTicks completion_time,
                   base::TimeDelta delay) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    mock_client_ = base::MakeRefCounted<MockRequestClient>();
    StartAsync(std::move(request), mock_client_);

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
                              std::move(consumer_handle), std::nullopt);
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
    EXPECT_TRUE(mock_client_->received_response());
    return mock_client_->last_load_timing().request_start;
  }
  base::TimeTicks completion_time() const {
    EXPECT_TRUE(mock_client_->complete());
    return mock_client_->completion_status().completion_time;
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

}  // namespace
}  // namespace blink
