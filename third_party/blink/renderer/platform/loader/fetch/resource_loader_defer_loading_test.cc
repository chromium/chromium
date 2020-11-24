// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

using ProcessCodeCacheRequestCallback =
    base::RepeatingCallback<void(WebCodeCacheLoader::FetchCodeCacheCallback)>;

// A mock code cache loader that calls the processing function whenever it
// receives fetch requests.
class TestCodeCacheLoader : public WebCodeCacheLoader {
 public:
  explicit TestCodeCacheLoader(ProcessCodeCacheRequestCallback callback)
      : process_request_(callback) {}
  ~TestCodeCacheLoader() override = default;

  // WebCodeCacheLoader methods:
  void FetchFromCodeCacheSynchronously(
      const WebURL& url,
      base::Time* response_time_out,
      mojo_base::BigBuffer* buffer_out) override {}
  void FetchFromCodeCache(
      blink::mojom::CodeCacheType cache_type,
      const WebURL& url,
      WebCodeCacheLoader::FetchCodeCacheCallback callback) override {
    process_request_.Run(std::move(callback));
  }

 private:
  ProcessCodeCacheRequestCallback process_request_;
};

// A mock WebURLLoader to know the status of defers flag.
class TestWebURLLoader final : public WebURLLoader {
 public:
  explicit TestWebURLLoader(WebURLLoader::DeferType* const defers_flag_ptr)
      : defers_flag_ptr_(defers_flag_ptr) {}
  ~TestWebURLLoader() override = default;

  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      int requestor_id,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    NOTREACHED();
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      int requestor_id,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient*) override {}

  void SetDefersLoading(WebURLLoader::DeferType defers) override {
    *defers_flag_ptr_ = defers;
  }
  void DidChangePriority(WebURLRequest::Priority, int) override {
    NOTREACHED();
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::web_url_loader_defers_|.
  WebURLLoader::DeferType* const defers_flag_ptr_;
};

class DeferTestLoaderFactory final : public ResourceFetcher::LoaderFactory {
 public:
  DeferTestLoaderFactory(
      WebURLLoader::DeferType* const defers_flag,
      ProcessCodeCacheRequestCallback process_code_cache_request_callback)
      : defers_flag_(defers_flag),
        process_code_cache_request_callback_(
            process_code_cache_request_callback) {}

  // LoaderFactory implementations
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner)
      override {
    return std::make_unique<TestWebURLLoader>(defers_flag_);
  }

  std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override {
    return std::make_unique<TestCodeCacheLoader>(
        process_code_cache_request_callback_);
  }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::web_url_loader_defers_|.
  WebURLLoader::DeferType* const defers_flag_;

  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
};

}  // namespace

class ResourceLoaderDefersLoadingTest : public testing::Test {
 public:
  ResourceLoaderDefersLoadingTest() {
    SetCodeCacheProcessFunction(base::BindRepeating(
        &ResourceLoaderDefersLoadingTest::SaveCodeCacheCallback,
        base::Unretained(this)));
  }

  void SaveCodeCacheCallback(
      WebCodeCacheLoader::FetchCodeCacheCallback callback) {
    // Store the callback to send back a response.
    code_cache_response_callback_ = std::move(callback);
  }

  ResourceFetcher* CreateFetcher() {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable(),
        MakeGarbageCollected<MockFetchContext>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<DeferTestLoaderFactory>(
            &web_url_loader_defers_, process_code_cache_request_callback_),
        MakeGarbageCollected<MockContextLifecycleNotifier>()));
  }

  void SetCodeCacheProcessFunction(ProcessCodeCacheRequestCallback callback) {
    process_code_cache_request_callback_ = callback;
  }

  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
  WebCodeCacheLoader::FetchCodeCacheCallback code_cache_response_callback_;
  // Passed to TestWebURLLoader (via |platform_|) and updated when its
  // SetDefersLoading method is called.
  WebURLLoader::DeferType web_url_loader_defers_ =
      WebURLLoader::DeferType::kNotDeferred;
  const KURL test_url_ = KURL("http://example.com/");

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(ResourceLoaderDefersLoadingTest, CodeCacheFetchCheckDefers) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);

  // After code cache fetch it should have deferred WebURLLoader.
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);
  DCHECK(resource);
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  // Once the response is received it should be reset.
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kNotDeferred);
}

TEST_F(ResourceLoaderDefersLoadingTest, CodeCacheFetchSyncReturn) {
  SetCodeCacheProcessFunction(base::BindRepeating(
      [](WebCodeCacheLoader::FetchCodeCacheCallback callback) {
        std::move(callback).Run(base::Time(), {});
      }));

  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK(resource);
  // The callback would be called so it should not be deferred.
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kNotDeferred);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToFalse) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  // Change Defers loading to false. This should not be sent to
  // WebURLLoader since a code cache request is still pending.
  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(WebURLLoader::DeferType::kNotDeferred);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToTrue) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(WebURLLoader::DeferType::kDeferred);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  std::move(code_cache_response_callback_).Run(base::Time(), {});
  // Since it was requested to be deferred, it should be reset to the
  // correct value.
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToBfcacheDefer) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(
      WebURLLoader::DeferType::kDeferredWithBackForwardCache);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  std::move(code_cache_response_callback_).Run(base::Time(), {});
  // Since it was requested to be deferred, it should be reset to the
  // correct value.
  DCHECK_EQ(web_url_loader_defers_,
            WebURLLoader::DeferType::kDeferredWithBackForwardCache);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersMultipleTimes) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(WebURLLoader::DeferType::kDeferred);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  loader->SetDefersLoading(WebURLLoader::DeferType::kNotDeferred);
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kDeferred);

  std::move(code_cache_response_callback_).Run(base::Time(), {});
  DCHECK_EQ(web_url_loader_defers_, WebURLLoader::DeferType::kNotDeferred);
}

}  // namespace blink
