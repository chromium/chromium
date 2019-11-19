// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "base/bind.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

class ResourceLoaderDefersLoadingTest : public testing::Test {
 public:
  using ProcessCodeCacheRequestCallback =
      base::RepeatingCallback<void(CodeCacheLoader::FetchCodeCacheCallback)>;
  class TestingPlatformSupportWithMockCodeCacheLoader;
  class TestCodeCacheLoader;
  class TestWebURLLoaderFactory;
  class TestWebURLLoader;

  ResourceLoaderDefersLoadingTest();

  void SaveCodeCacheCallback(CodeCacheLoader::FetchCodeCacheCallback callback) {
    // Store the callback to send back a response.
    code_cache_response_callback_ = std::move(callback);
  }

  ResourceFetcher* CreateFetcher() {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable(),
        MakeGarbageCollected<MockFetchContext>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>()));
  }

  CodeCacheLoader::FetchCodeCacheCallback code_cache_response_callback_;
  // Passed to TestWebURLLoader (via |platform_|) and updated when its
  // SetDefersLoading method is called.
  bool web_url_loader_defers_ = false;
  const KURL test_url_;

  ScopedTestingPlatformSupport<
      ResourceLoaderDefersLoadingTest::
          TestingPlatformSupportWithMockCodeCacheLoader,
      bool*>
      platform_;
};

// A mock code cache loader that calls the processing function whenever it
// receives fetch requests.
class ResourceLoaderDefersLoadingTest::TestCodeCacheLoader
    : public CodeCacheLoader {
 public:
  explicit TestCodeCacheLoader(ProcessCodeCacheRequestCallback callback)
      : process_request_(callback) {}
  ~TestCodeCacheLoader() override = default;

  // CodeCacheLoader methods:
  void FetchFromCodeCacheSynchronously(
      const GURL& url,
      base::Time* response_time_out,
      mojo_base::BigBuffer* buffer_out) override {}
  void FetchFromCodeCache(
      blink::mojom::CodeCacheType cache_type,
      const GURL& url,
      CodeCacheLoader::FetchCodeCacheCallback callback) override {
    process_request_.Run(std::move(callback));
  }

 private:
  ProcessCodeCacheRequestCallback process_request_;
};

// A mock WebURLLoader to know the status of defers flag.
class ResourceLoaderDefersLoadingTest::TestWebURLLoader final
    : public WebURLLoader {
 public:
  explicit TestWebURLLoader(bool* const defers_flag_ptr)
      : defers_flag_ptr_(defers_flag_ptr) {}
  ~TestWebURLLoader() override = default;

  void LoadSynchronously(const WebURLRequest&,
                         WebURLLoaderClient*,
                         WebURLResponse&,
                         base::Optional<WebURLError>&,
                         WebData&,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length,
                         WebBlobInfo& downloaded_blob) override {
    NOTREACHED();
  }
  void LoadAsynchronously(const WebURLRequest&, WebURLLoaderClient*) override {}

  void SetDefersLoading(bool defers) override { *defers_flag_ptr_ = defers; }
  void DidChangePriority(WebURLRequest::Priority, int) override {
    NOTREACHED();
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::web_url_loader_defers_|.
  bool* const defers_flag_ptr_;
};

// Mock WebURLLoaderFactory.
class ResourceLoaderDefersLoadingTest::TestWebURLLoaderFactory final
    : public WebURLLoaderFactory {
 public:
  explicit TestWebURLLoaderFactory(bool* const defers_flag)
      : defers_flag_(defers_flag) {}

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest& request,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>) override {
    return std::make_unique<TestWebURLLoader>(defers_flag_);
  }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::web_url_loader_defers_|.
  bool* const defers_flag_;
};

// Mock TestPlatform to create the specific WebURLLoaderFactory and
// CodeCacheLoader required for the tests.
class ResourceLoaderDefersLoadingTest::
    TestingPlatformSupportWithMockCodeCacheLoader
    : public TestingPlatformSupportWithMockScheduler {
 public:
  TestingPlatformSupportWithMockCodeCacheLoader(bool* const defers_flag)
      : defers_flag_(defers_flag) {}

  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override {
    return std::make_unique<TestCodeCacheLoader>(process_code_cache_request_);
  }

  std::unique_ptr<WebURLLoaderFactory> CreateDefaultURLLoaderFactory()
      override {
    return std::make_unique<TestWebURLLoaderFactory>(defers_flag_);
  }

  void SetCodeCacheProcessFunction(ProcessCodeCacheRequestCallback callback) {
    process_code_cache_request_ = callback;
  }

 private:
  ProcessCodeCacheRequestCallback process_code_cache_request_;
  // Points to |ResourceLoaderDefersLoadingTest::web_url_loader_defers_|.
  bool* const defers_flag_;
};

ResourceLoaderDefersLoadingTest::ResourceLoaderDefersLoadingTest()
    : test_url_("http://example.com/"), platform_(&web_url_loader_defers_) {
  // Saves the callback to control when the response is sent from
  // the code cache loader.
  platform_->SetCodeCacheProcessFunction(base::BindRepeating(
      &ResourceLoaderDefersLoadingTest::SaveCodeCacheCallback,
      base::Unretained(this)));
}

TEST_F(ResourceLoaderDefersLoadingTest, CodeCacheFetchCheckDefers) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters fetch_parameters(request);

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);

  // After code cache fetch it should have deferred WebURLLoader.
  DCHECK(web_url_loader_defers_);
  DCHECK(resource);
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  // Once the response is received it should be reset.
  DCHECK(!web_url_loader_defers_);
}

TEST_F(ResourceLoaderDefersLoadingTest, CodeCacheFetchSyncReturn) {
  platform_->SetCodeCacheProcessFunction(
      base::BindRepeating([](CodeCacheLoader::FetchCodeCacheCallback callback) {
        std::move(callback).Run(base::Time(), {});
      }));

  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters fetch_parameters(request);

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK(resource);
  // The callback would be called so it should not be deferred.
  DCHECK(!web_url_loader_defers_);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToFalse) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters fetch_parameters(request);

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK(web_url_loader_defers_);

  // Change Defers loading to false. This should not be sent to
  // WebURLLoader since a code cache request is still pending.
  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(false);
  DCHECK(web_url_loader_defers_);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToTrue) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters fetch_parameters(request);

  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK(web_url_loader_defers_);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(true);
  DCHECK(web_url_loader_defers_);

  std::move(code_cache_response_callback_).Run(base::Time(), {});
  // Since it was requested to be deferred, it should be reset to the
  // correct value.
  DCHECK(web_url_loader_defers_);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersMultipleTimes) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::RequestContextType::FETCH);

  FetchParameters fetch_parameters(request);
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK(web_url_loader_defers_);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(true);
  DCHECK(web_url_loader_defers_);

  loader->SetDefersLoading(false);
  DCHECK(web_url_loader_defers_);

  std::move(code_cache_response_callback_).Run(base::Time(), {});
  DCHECK(!web_url_loader_defers_);
}

}  // namespace blink
