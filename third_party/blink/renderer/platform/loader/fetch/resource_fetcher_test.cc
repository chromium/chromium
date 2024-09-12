/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/testing/fetch_testing_platform_support.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory_impl.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

constexpr char kTestResourceFilename[] = "white-1x1.png";
constexpr char kTestResourceMimeType[] = "image/png";

class PartialResourceRequest {
 public:
  PartialResourceRequest() : PartialResourceRequest(ResourceRequest()) {}
  PartialResourceRequest(const ResourceRequest& request)
      : is_ad_resource_(request.IsAdResource()),
        priority_(request.Priority()) {}

  bool IsAdResource() const { return is_ad_resource_; }
  ResourceLoadPriority Priority() const { return priority_; }

 private:
  bool is_ad_resource_;
  ResourceLoadPriority priority_;
};

}  // namespace

class ResourceFetcherTest : public testing::Test {
 public:
  ResourceFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    Resource::SetClockForTesting(task_environment_.GetMockClock());
    // The state of global LcppEnabled flag depends on several feature flags
    // which can be enabled/disabled in tests. Clear the global flag value.
    ResetLcppEnabledForTesting();
  }
  ~ResourceFetcherTest() override {
    MemoryCache::Get()->EvictResources();
    Resource::SetClockForTesting(nullptr);
  }

  ResourceFetcherTest(const ResourceFetcherTest&) = delete;
  ResourceFetcherTest& operator=(const ResourceFetcherTest&) = delete;

  class TestResourceLoadObserver final : public ResourceLoadObserver {
   public:
    // ResourceLoadObserver implementation.
    void DidStartRequest(const FetchParameters&, ResourceType) override {}
    void WillSendRequest(const ResourceRequest& request,
                         const ResourceResponse& redirect_response,
                         ResourceType,
                         const ResourceLoaderOptions&,
                         RenderBlockingBehavior,
                         const Resource*) override {
      request_ = PartialResourceRequest(request);
    }
    void DidChangePriority(uint64_t identifier,
                           ResourceLoadPriority,
                           int intra_priority_value) override {}
    void DidReceiveResponse(uint64_t identifier,
                            const ResourceRequest& request,
                            const ResourceResponse& response,
                            const Resource* resource,
                            ResponseSource source) override {}
    void DidReceiveData(uint64_t identifier,
                        base::SpanOrSize<const char> chunk) override {}
    void DidReceiveTransferSizeUpdate(uint64_t identifier,
                                      int transfer_size_diff) override {}
    void DidDownloadToBlob(uint64_t identifier, BlobDataHandle*) override {}
    void DidFinishLoading(uint64_t identifier,
                          base::TimeTicks finish_time,
                          int64_t encoded_data_length,
                          int64_t decoded_body_length) override {}
    void DidFailLoading(const KURL&,
                        uint64_t identifier,
                        const ResourceError&,
                        int64_t encoded_data_length,
                        IsInternalRequest is_internal_request) override {}
    void DidChangeRenderBlockingBehavior(
        Resource* resource,
        const FetchParameters& params) override {}
    bool InterestedInAllRequests() override {
      return interested_in_all_requests_;
    }
    void SetInterestedInAllRequests(bool interested_in_all_requests) {
      interested_in_all_requests_ = interested_in_all_requests;
    }
    const std::optional<PartialResourceRequest>& GetLastRequest() const {
      return request_;
    }

    void ClearLastRequest() { request_ = std::nullopt; }

   private:
    std::optional<PartialResourceRequest> request_;
    bool interested_in_all_requests_ = false;
  };

 protected:
  scoped_refptr<scheduler::FakeTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

  ResourceFetcher* CreateFetcher(
      const TestResourceFetcherProperties& properties,
      FetchContext* context) {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties.MakeDetachable(), context, CreateTaskRunner(),
        CreateTaskRunner(),
        MakeGarbageCollected<TestLoaderFactory>(
            platform_->GetURLLoaderMockFactory()),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
  }

  ResourceFetcher* CreateFetcher(
      const TestResourceFetcherProperties& properties) {
    return CreateFetcher(properties, MakeGarbageCollected<MockFetchContext>());
  }

  ResourceFetcher* CreateFetcher() {
    return CreateFetcher(
        *MakeGarbageCollected<TestResourceFetcherProperties>());
  }

  void AddResourceToMemoryCache(Resource* resource) {
    MemoryCache::Get()->Add(resource);
  }

  void RegisterMockedURLLoad(const KURL& url) {
    url_test_helpers::RegisterMockedURLLoad(
        url, test::PlatformTestDataPath(kTestResourceFilename),
        kTestResourceMimeType, platform_->GetURLLoaderMockFactory());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach) {
  KURL secure_url("https://secureorigin.test/image.png");
  // Try to request a url. The request should fail, and a resource in an error
  // state should be returned, and no resource should be present in the cache.
  auto* fetcher = CreateFetcher();
  fetcher->ClearContext();
  ResourceRequest resource_request(secure_url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  Resource* resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->ErrorOccurred());
  EXPECT_TRUE(resource->GetResourceError().IsAccessCheck());
  EXPECT_FALSE(MemoryCache::Get()->ResourceForURLForTesting(secure_url));

  // Start by calling StartLoad() directly, rather than via RequestResource().
  // This shouldn't crash. Setting the resource type to image, as StartLoad with
  // a single argument is only called on images or fonts.
  fetcher->StartLoad(RawResource::CreateForTest(
      secure_url, SecurityOrigin::CreateUniqueOpaque(), ResourceType::kImage));
}

TEST_F(ResourceFetcherTest, UseExistingResource) {
  base::HistogramTester histogram_tester;
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);

  // Test histograms.
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    2);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      3 /* RevalidationPolicy::kLoad */, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 1);

  // Create a new fetcher and load the same resource.
  auto* new_fetcher = CreateFetcher();
  Resource* new_fetcher_resource =
      MockResource::Fetch(fetch_params, new_fetcher, nullptr);
  EXPECT_EQ(resource, new_fetcher_resource);
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    3);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      3 /* RevalidationPolicy::kLoad */, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 2);
}

TEST_F(ResourceFetcherTest, MetricsPerTopFrameSite) {
  base::HistogramTester histogram_tester;

  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  ResourceRequestHead request_head(url);
  scoped_refptr<const SecurityOrigin> origin_a =
      SecurityOrigin::Create(KURL("https://a.test"));
  request_head.SetTopFrameOrigin(origin_a);
  request_head.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(request_head));
  auto* fetcher_1 = CreateFetcher();
  Resource* resource_1 = MockResource::Fetch(fetch_params, fetcher_1, nullptr);
  ASSERT_TRUE(resource_1);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource_1->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource_1));

  auto* fetcher_2 = CreateFetcher();
  ResourceRequestHead request_head_2(url);
  scoped_refptr<const SecurityOrigin> origin_b =
      SecurityOrigin::Create(KURL("https://b.test"));
  request_head_2.SetTopFrameOrigin(origin_b);
  request_head_2.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params_2 =
      FetchParameters::CreateForTest(ResourceRequest(request_head_2));
  Resource* resource_2 =
      MockResource::Fetch(fetch_params_2, fetcher_2, nullptr);
  EXPECT_EQ(resource_1, resource_2);

  // Test histograms.
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    2);

  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      3 /* RevalidationPolicy::kLoad */, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 1);

  // Now load the same resource with origin_b as top-frame site. The
  // histograms should be incremented.
  auto* fetcher_3 = CreateFetcher();
  ResourceRequestHead request_head_3(url);
  scoped_refptr<const SecurityOrigin> foo_origin_b =
      SecurityOrigin::Create(KURL("https://foo.b.test"));
  request_head_3.SetTopFrameOrigin(foo_origin_b);
  request_head_3.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params_3 =
      FetchParameters::CreateForTest(ResourceRequest(request_head_3));
  Resource* resource_3 =
      MockResource::Fetch(fetch_params_2, fetcher_3, nullptr);
  EXPECT_EQ(resource_1, resource_3);
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    3);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 2);
}

TEST_F(ResourceFetcherTest, MetricsPerTopFrameSiteOpaqueOrigins) {
  base::HistogramTester histogram_tester;

  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  ResourceRequestHead request_head(url);
  scoped_refptr<const SecurityOrigin> origin_a =
      SecurityOrigin::Create(KURL("https://a.test"));
  scoped_refptr<const SecurityOrigin> opaque_origin1 =
      SecurityOrigin::CreateUniqueOpaque();
  request_head.SetTopFrameOrigin(opaque_origin1);
  request_head.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(request_head));
  auto* fetcher_1 = CreateFetcher();
  Resource* resource_1 = MockResource::Fetch(fetch_params, fetcher_1, nullptr);
  ASSERT_TRUE(resource_1);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource_1->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource_1));

  // Create a 2nd opaque top-level origin.
  auto* fetcher_2 = CreateFetcher();
  ResourceRequestHead request_head_2(url);
  scoped_refptr<const SecurityOrigin> opaque_origin2 =
      SecurityOrigin::CreateUniqueOpaque();
  request_head_2.SetTopFrameOrigin(opaque_origin2);
  request_head_2.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params_2 =
      FetchParameters::CreateForTest(ResourceRequest(request_head_2));
  Resource* resource_2 =
      MockResource::Fetch(fetch_params_2, fetcher_2, nullptr);
  EXPECT_EQ(resource_1, resource_2);

  // Test histograms.
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    2);

  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      3 /* RevalidationPolicy::kLoad */, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 1);

  // Now load the same resource with opaque_origin1 as top-frame site. The
  // histograms should be incremented.
  auto* fetcher_3 = CreateFetcher();
  ResourceRequestHead request_head_3(url);
  request_head_3.SetTopFrameOrigin(opaque_origin2);
  request_head_3.SetRequestorOrigin(origin_a);
  FetchParameters fetch_params_3 =
      FetchParameters::CreateForTest(ResourceRequest(request_head_3));
  Resource* resource_3 =
      MockResource::Fetch(fetch_params_2, fetcher_3, nullptr);
  EXPECT_EQ(resource_1, resource_3);
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Mock",
                                    3);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Mock",
      0 /* RevalidationPolicy::kUse */, 2);
}

// Verify that the ad bit is copied to WillSendRequest's request when the
// response is served from the memory cache.
TEST_F(ResourceFetcherTest, WillSendRequestAdBit) {
  // Add a resource to the memory cache.
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();
  auto* properties =
      MakeGarbageCollected<TestResourceFetcherProperties>(source_origin);
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  KURL url("http://127.0.0.1:8000/foo.html");
  Resource* resource =
      RawResource::CreateForTest(url, source_origin, ResourceType::kRaw);
  AddResourceToMemoryCache(resource);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  resource->ResponseReceived(response);
  resource->FinishForTest();

  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();
  // Fetch the cached resource. The request to DispatchWillSendRequest should
  // preserve the ad bit.
  auto* fetcher = CreateFetcher(*properties, context);
  fetcher->SetResourceLoadObserver(observer);
  ResourceRequest resource_request(url);
  resource_request.SetIsAdResource();
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  platform_->GetURLLoaderMockFactory()->RegisterURL(url, WebURLResponse(), "");
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher, nullptr);

  EXPECT_EQ(resource, new_resource);
  std::optional<PartialResourceRequest> new_request =
      observer->GetLastRequest();
  EXPECT_TRUE(new_request.has_value());
  EXPECT_TRUE(new_request.value().IsAdResource());
}

TEST_F(ResourceFetcherTest, Vary) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();
  KURL url("http://127.0.0.1:8000/foo.html");
  Resource* resource =
      RawResource::CreateForTest(url, source_origin, ResourceType::kRaw);
  AddResourceToMemoryCache(resource);

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  response.SetHttpHeaderField(http_names::kVary, AtomicString("*"));
  resource->ResponseReceived(response);
  resource->FinishForTest();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  auto* fetcher = CreateFetcher(
      *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin));
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  platform_->GetURLLoaderMockFactory()->RegisterURL(url, WebURLResponse(), "");
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_NE(resource, new_resource);
  new_resource->Loader()->Cancel();
}

TEST_F(ResourceFetcherTest, VaryOnBack) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();
  auto* fetcher = CreateFetcher(
      *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin));

  KURL url("http://127.0.0.1:8000/foo.html");
  Resource* resource =
      RawResource::CreateForTest(url, source_origin, ResourceType::kRaw);
  AddResourceToMemoryCache(resource);

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  response.SetHttpHeaderField(http_names::kVary, AtomicString("*"));
  resource->ResponseReceived(response);
  resource->FinishForTest();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  ResourceRequest resource_request(url);
  resource_request.SetCacheMode(mojom::FetchCacheMode::kForceCache);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);
}

TEST_F(ResourceFetcherTest, VaryResource) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  response.SetHttpHeaderField(http_names::kVary, AtomicString("*"));
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);
}

class RequestSameResourceOnComplete
    : public GarbageCollected<RequestSameResourceOnComplete>,
      public RawResourceClient {
 public:
  RequestSameResourceOnComplete(URLLoaderMockFactory* mock_factory,
                                FetchParameters& params,
                                ResourceFetcher* fetcher)
      : mock_factory_(mock_factory),
        source_origin_(fetcher->GetProperties()
                           .GetFetchClientSettingsObject()
                           .GetSecurityOrigin()) {
    MockResource::Fetch(params, fetcher, this);
  }

  void NotifyFinished(Resource* resource) override {
    EXPECT_EQ(GetResource(), resource);
    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(source_origin_);
    MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* fetcher2 = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context,
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>(mock_factory_),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
    ResourceRequest resource_request2(GetResource()->Url());
    resource_request2.SetCacheMode(mojom::FetchCacheMode::kValidateCache);
    FetchParameters fetch_params2 =
        FetchParameters::CreateForTest(std::move(resource_request2));
    Resource* resource2 = MockResource::Fetch(fetch_params2, fetcher2, nullptr);
    EXPECT_EQ(GetResource(), resource2);
    notify_finished_called_ = true;
    ClearResource();
  }
  bool NotifyFinishedCalled() const { return notify_finished_called_; }

  void Trace(Visitor* visitor) const override {
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "RequestSameResourceOnComplete"; }

 private:
  raw_ptr<URLLoaderMockFactory> mock_factory_;
  bool notify_finished_called_ = false;
  scoped_refptr<const SecurityOrigin> source_origin_;
};

TEST_F(ResourceFetcherTest, DISABLED_RevalidateWhileFinishingLoading) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();
  KURL url("http://127.0.0.1:8000/foo.png");

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=3600"));
  response.SetHttpHeaderField(http_names::kETag, AtomicString("1234567890"));
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  ResourceFetcher* fetcher1 = CreateFetcher(
      *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin));
  ResourceRequest request1(url);
  request1.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("no-cache"));
  FetchParameters fetch_params1 =
      FetchParameters::CreateForTest(std::move(request1));
  Persistent<RequestSameResourceOnComplete> client =
      MakeGarbageCollected<RequestSameResourceOnComplete>(
          platform_->GetURLLoaderMockFactory(), fetch_params1, fetcher1);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(client->NotifyFinishedCalled());
}

// TODO(crbug.com/850785): Reenable this.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DontReuseMediaDataUrl DISABLED_DontReuseMediaDataUrl
#else
#define MAYBE_DontReuseMediaDataUrl DontReuseMediaDataUrl
#endif
TEST_F(ResourceFetcherTest, MAYBE_DontReuseMediaDataUrl) {
  auto* fetcher = CreateFetcher();
  ResourceRequest request(KURL("data:text/html,foo"));
  request.SetRequestContext(mojom::blink::RequestContextType::VIDEO);
  ResourceLoaderOptions options(nullptr /* world */);
  options.data_buffering_policy = kDoNotBufferData;
  options.initiator_info.name = fetch_initiator_type_names::kInternal;
  FetchParameters fetch_params(std::move(request), options);
  Resource* resource1 = RawResource::FetchMedia(fetch_params, fetcher, nullptr);
  Resource* resource2 = RawResource::FetchMedia(fetch_params, fetcher, nullptr);
  EXPECT_NE(resource1, resource2);
}

class ServeRequestsOnCompleteClient final
    : public GarbageCollected<ServeRequestsOnCompleteClient>,
      public RawResourceClient {
 public:
  explicit ServeRequestsOnCompleteClient(URLLoaderMockFactory* mock_factory)
      : mock_factory_(mock_factory) {}

  void NotifyFinished(Resource*) override {
    mock_factory_->ServeAsynchronousRequests();
    ClearResource();
  }

  // No callbacks should be received except for the NotifyFinished() triggered
  // by ResourceLoader::Cancel().
  void DataSent(Resource*, uint64_t, uint64_t) override { ASSERT_TRUE(false); }
  void ResponseReceived(Resource*, const ResourceResponse&) override {
    ASSERT_TRUE(false);
  }
  void CachedMetadataReceived(Resource*, mojo_base::BigBuffer) override {
    ASSERT_TRUE(false);
  }
  void DataReceived(Resource*, base::span<const char>) override {
    ASSERT_TRUE(false);
  }
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ADD_FAILURE();
    return true;
  }
  void DataDownloaded(Resource*, uint64_t) override { ASSERT_TRUE(false); }

  void Trace(Visitor* visitor) const override {
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "ServeRequestsOnCompleteClient"; }

 private:
  raw_ptr<URLLoaderMockFactory, DanglingUntriaged> mock_factory_;
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::Cancel(). If the ResourceLoader doesn't promptly cancel its
// URLLoader before notifying its clients, a nested run loop  may send a network
// response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel) {
  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  auto* fetcher = CreateFetcher();
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  Persistent<ServeRequestsOnCompleteClient> client =
      MakeGarbageCollected<ServeRequestsOnCompleteClient>(
          platform_->GetURLLoaderMockFactory());
  Resource* resource = RawResource::Fetch(fetch_params, fetcher, client);
  resource->Loader()->Cancel();
}

class ScopedMockRedirectRequester {
  STACK_ALLOCATED();

 public:
  ScopedMockRedirectRequester(
      URLLoaderMockFactory* mock_factory,
      MockFetchContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : mock_factory_(mock_factory),
        context_(context),
        task_runner_(std::move(task_runner)) {}
  ScopedMockRedirectRequester(const ScopedMockRedirectRequester&) = delete;
  ScopedMockRedirectRequester& operator=(const ScopedMockRedirectRequester&) =
      delete;

  void RegisterRedirect(const WebString& from_url, const WebString& to_url) {
    KURL redirect_url(from_url);
    WebURLResponse redirect_response;
    redirect_response.SetCurrentRequestUrl(redirect_url);
    redirect_response.SetHttpStatusCode(301);
    redirect_response.SetHttpHeaderField(http_names::kLocation, to_url);
    redirect_response.SetEncodedDataLength(kRedirectResponseOverheadBytes);

    mock_factory_->RegisterURL(redirect_url, redirect_response, "");
  }

  void RegisterFinalResource(const WebString& url) {
    url_test_helpers::RegisterMockedURLLoad(
        KURL(url), test::PlatformTestDataPath(kTestResourceFilename),
        kTestResourceMimeType, mock_factory_);
  }

  void Request(const WebString& url) {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context_, task_runner_,
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>(mock_factory_),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::INTERNAL);
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(resource_request));
    RawResource::Fetch(fetch_params, fetcher, nullptr);
    mock_factory_->ServeAsynchronousRequests();
  }

 private:
  URLLoaderMockFactory* mock_factory_;
  MockFetchContext* context_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

TEST_F(ResourceFetcherTest, SynchronousRequest) {
  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  auto* fetcher = CreateFetcher();
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  fetch_params.MakeSynchronous();
  Resource* resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_EQ(ResourceLoadPriority::kHighest,
            resource->GetResourceRequest().Priority());
}

TEST_F(ResourceFetcherTest, PingPriority) {
  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  auto* fetcher = CreateFetcher();
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(mojom::blink::RequestContextType::PING);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  Resource* resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow,
            resource->GetResourceRequest().Priority());
}

TEST_F(ResourceFetcherTest, PreloadResourceTwice) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_original.SetLinkPreload(true);
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource));
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params.SetLinkPreload(true);
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource));

  fetcher->ClearPreloads(ResourceFetcher::kClearAllPreloads);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource));
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource));
  EXPECT_TRUE(resource->IsUnusedPreload());
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceAndUse) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  // Link preload preload scanner
  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_original.SetLinkPreload(true);
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // Resource created by preload scanner
  FetchParameters fetch_params_preload_scanner =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* preload_scanner_resource =
      MockResource::Fetch(fetch_params_preload_scanner, fetcher, nullptr);
  EXPECT_EQ(resource, preload_scanner_resource);
  EXPECT_TRUE(resource->IsLinkPreload());

  // Resource created by parser
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher, client);
  EXPECT_EQ(resource, new_resource);
  EXPECT_TRUE(resource->IsLinkPreload());

  // DCL reached
  fetcher->ClearPreloads(ResourceFetcher::kClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
  EXPECT_FALSE(resource->IsUnusedPreload());
}

TEST_F(ResourceFetcherTest, PreloadMatchWithBypassingCache) {
  auto* fetcher = CreateFetcher();
  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_original.SetLinkPreload(true);
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params_second =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_second.MutableResourceRequest().SetCacheMode(
      mojom::FetchCacheMode::kBypassCache);
  Resource* second_resource =
      MockResource::Fetch(fetch_params_second, fetcher, nullptr);
  EXPECT_EQ(resource, second_resource);
  EXPECT_TRUE(resource->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, CrossFramePreloadMatchIsNotAllowed) {
  auto* fetcher = CreateFetcher();
  auto* fetcher2 = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_original.SetLinkPreload(true);
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params_second =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_second.MutableResourceRequest().SetCacheMode(
      mojom::FetchCacheMode::kBypassCache);
  Resource* second_resource =
      MockResource::Fetch(fetch_params_second, fetcher2, nullptr);

  EXPECT_NE(resource, second_resource);
  EXPECT_TRUE(resource->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, RepetitiveLinkPreloadShouldBeMerged) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request =
      FetchParameters::CreateForTest(ResourceRequest(url));
  FetchParameters fetch_params_for_preload =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_preload.SetLinkPreload(true);

  Resource* resource1 =
      MockResource::Fetch(fetch_params_for_preload, fetcher, nullptr);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returns the first preload.
  Resource* resource2 =
      MockResource::Fetch(fetch_params_for_preload, fetcher, nullptr);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 =
      MockResource::Fetch(fetch_params_for_request, fetcher, nullptr);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsUnusedPreload());
}

TEST_F(ResourceFetcherTest, RepetitiveSpeculativePreloadShouldBeMerged) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request =
      FetchParameters::CreateForTest(ResourceRequest(url));
  FetchParameters fetch_params_for_preload =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_preload.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);

  Resource* resource1 =
      MockResource::Fetch(fetch_params_for_preload, fetcher, nullptr);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returns the first preload.
  Resource* resource2 =
      MockResource::Fetch(fetch_params_for_preload, fetcher, nullptr);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 =
      MockResource::Fetch(fetch_params_for_request, fetcher, nullptr);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsUnusedPreload());
}

TEST_F(ResourceFetcherTest, SpeculativePreloadShouldBePromotedToLinkPreload) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request =
      FetchParameters::CreateForTest(ResourceRequest(url));
  FetchParameters fetch_params_for_speculative_preload =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_speculative_preload.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);
  FetchParameters fetch_params_for_link_preload =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_link_preload.SetLinkPreload(true);

  Resource* resource1 = MockResource::Fetch(
      fetch_params_for_speculative_preload, fetcher, nullptr);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_FALSE(resource1->IsLinkPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returns the first preload.
  Resource* resource2 =
      MockResource::Fetch(fetch_params_for_link_preload, fetcher, nullptr);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_TRUE(resource1->IsLinkPreload());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 =
      MockResource::Fetch(fetch_params_for_request, fetcher, nullptr);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsUnusedPreload());
  EXPECT_TRUE(resource1->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, Revalidate304) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();

  KURL url("http://127.0.0.1:8000/foo.html");
  Resource* resource =
      RawResource::CreateForTest(url, source_origin, ResourceType::kRaw);
  AddResourceToMemoryCache(resource);

  ResourceResponse response(url);
  response.SetHttpStatusCode(304);
  response.SetHttpHeaderField(http_names::kETag, AtomicString("1234567890"));
  resource->ResponseReceived(response);
  resource->FinishForTest();

  auto* fetcher = CreateFetcher(
      *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin));
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(resource_request));
  platform_->GetURLLoaderMockFactory()->RegisterURL(url, WebURLResponse(), "");
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher, nullptr);
  fetcher->StopFetching();

  EXPECT_NE(resource, new_resource);
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceMultipleFetchersAndMove) {
  auto* fetcher = CreateFetcher();
  auto* fetcher2 = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_original.SetLinkPreload(true);
  Resource* resource =
      MockResource::Fetch(fetch_params_original, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  EXPECT_EQ(0, fetcher->BlockingRequestCount());

  // Resource created by parser on the second fetcher
  FetchParameters fetch_params2 =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Persistent<MockResourceClient> client2 =
      MakeGarbageCollected<MockResourceClient>();
  Resource* new_resource2 =
      MockResource::Fetch(fetch_params2, fetcher2, client2);
  EXPECT_NE(resource, new_resource2);
  EXPECT_EQ(0, fetcher2->BlockingRequestCount());
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
}

// TODO(crbug.com/850785): Reenable this.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ContentTypeDataURL DISABLED_ContentTypeDataURL
#else
#define MAYBE_ContentTypeDataURL ContentTypeDataURL
#endif
TEST_F(ResourceFetcherTest, MAYBE_ContentTypeDataURL) {
  auto* fetcher = CreateFetcher();
  FetchParameters fetch_params = FetchParameters::CreateForTest(
      ResourceRequest("data:text/testmimetype,foo"));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  EXPECT_EQ(ResourceStatus::kCached, resource->GetStatus());
  EXPECT_EQ("text/testmimetype", resource->GetResponse().MimeType());
  EXPECT_EQ("text/testmimetype", resource->GetResponse().HttpContentType());
}

// Request with the Content-ID scheme must not be canceled, even if there is no
// MHTMLArchive to serve them.
// Note: Not blocking it is important because there are some embedders of
// Android WebView that are intercepting Content-ID URLs and serve their own
// resources. Please see https://crbug.com/739658.
TEST_F(ResourceFetcherTest, ContentIdURL) {
  KURL url("cid:0123456789@example.com");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  auto* fetcher = CreateFetcher();

  // Subresource case.
  {
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(mojom::blink::RequestContextType::VIDEO);
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(resource_request));
    RawResource* resource =
        RawResource::FetchMedia(fetch_params, fetcher, nullptr);
    ASSERT_NE(nullptr, resource);
    EXPECT_FALSE(resource->ErrorOccurred());
  }
}

TEST_F(ResourceFetcherTest, StaleWhileRevalidate) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();
  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = CreateFetcher(
      *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin),
      context);
  fetcher->SetResourceLoadObserver(observer);

  KURL url("http://127.0.0.1:8000/foo.html");
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kCacheControl,
      AtomicString("max-age=0, stale-while-revalidate=40"));

  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(
      mojom::blink::RequestContextType::INTERNAL);
  FetchParameters fetch_params2 =
      FetchParameters::CreateForTest(std::move(resource_request));
  Resource* new_resource = MockResource::Fetch(fetch_params2, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());

  // Advance the clock, make sure the original resource gets removed from the
  // memory cache after the revalidation completes.
  task_environment_.AdvanceClock(base::Seconds(1));
  ResourceResponse revalidate_response(url);
  revalidate_response.SetHttpStatusCode(200);
  platform_->GetURLLoaderMockFactory()->UnregisterURL(url);
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(revalidate_response),
      test::PlatformTestDataPath(kTestResourceFilename));
  new_resource = MockResource::Fetch(fetch_params2, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();
  std::optional<PartialResourceRequest> swr_request =
      observer->GetLastRequest();
  ASSERT_TRUE(swr_request.has_value());
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, swr_request->Priority());
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource));
}

TEST_F(ResourceFetcherTest, CachedResourceShouldNotCrashByNullURL) {
  auto* fetcher = CreateFetcher();

  // Make sure |cached_resources_map_| is not empty, so that HashMap lookup
  // won't take a fast path.
  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_NE(fetcher->CachedResource(url), nullptr);

  ASSERT_EQ(fetcher->CachedResource(KURL()), nullptr);
}

TEST_F(ResourceFetcherTest, DeprioritizeSubframe) {
  auto& properties = *MakeGarbageCollected<TestResourceFetcherProperties>();
  auto* fetcher = CreateFetcher(properties);
  ResourceRequest request(KURL("https://www.example.com/"));

  {
    // Subframe deprioritization is disabled (main frame case).
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kScript, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kHigh);
  }

  {
    // Subframe deprioritization is disabled (nested frame case).
    properties.SetIsOutermostMainFrame(false);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kScript, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kHigh);
  }

  {
    // Subframe deprioritization is enabled (main frame case), kHigh.
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(true);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kScript, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kHigh);
  }

  {
    // Subframe deprioritization is enabled (nested frame case), kHigh => kLow.
    properties.SetIsOutermostMainFrame(false);
    properties.SetIsSubframeDeprioritizationEnabled(true);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kScript, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kLow);
  }
  {
    // Subframe deprioritization is enabled (main frame case), kMedium.
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(true);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kMock, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kMedium);
  }

  {
    // Subframe deprioritization is enabled (nested frame case), kMedium =>
    // kLowest.
    properties.SetIsOutermostMainFrame(false);
    properties.SetIsSubframeDeprioritizationEnabled(true);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kMock, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kLowest);
  }
}

TEST_F(ResourceFetcherTest, BoostImagePriority) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBoostImagePriority);
  auto& properties = *MakeGarbageCollected<TestResourceFetcherProperties>();
  auto* fetcher = CreateFetcher(properties);
  ResourceRequest request(KURL("https://www.example.com/"));

  // A "small" image should not get a priority boost or count against the
  // 5-image limit.
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        10 /* resource_width*/, 10 /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kLow);
  }

  // Test an image with just one of width or height set to zero but the other
  // dimension not specified to make sure it is also treated as "small"
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        0 /* resource_width*/, std::nullopt /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kLow);
  }
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        std::nullopt /* resource_width*/, 0 /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kLow);
  }

  // The next 5 images that are not-small should be boosted to Medium priority.
  // Test both an explicit size over 10,000px^2 as well as no size specified
  // which defaults to not-small.
  // #1 - 200x200 = 40000px^2.
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        200 /* resource_width*/, 200 /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kMedium);
  }
  // #2 - non-zero width but no height.
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        200 /* resource_width*/, std::nullopt /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kMedium);
  }
  // #3 - non-zero height but no width.
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */,
        std::nullopt /* resource_width*/, 200 /* resource_height*/);
    EXPECT_EQ(priority, ResourceLoadPriority::kMedium);
  }
  // #4-5 - neither height nor width.
  for (int i = 4; i <= 5; i++) {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kMedium);
  }

  // After the 5th non-small image, images should get the default Low priority.
  {
    properties.SetIsOutermostMainFrame(true);
    properties.SetIsSubframeDeprioritizationEnabled(false);
    const auto priority = fetcher->ComputeLoadPriorityForTesting(
        ResourceType::kImage, request, ResourcePriority::kNotVisible,
        FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kInDocument,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false /* is_link_preload */);
    EXPECT_EQ(priority, ResourceLoadPriority::kLow);
  }
}

TEST_F(ResourceFetcherTest, IsPotentiallyLCPElement) {
  for (const auto& test_cases :
       {std::make_pair("medium", ResourceLoadPriority::kMedium),
        std::make_pair("high", ResourceLoadPriority::kHigh),
        std::make_pair("very_high", ResourceLoadPriority::kVeryHigh)}) {
    const char* kPrioritySetting = test_cases.first;
    const ResourceLoadPriority kExpectedPriority = test_cases.second;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{features::kLCPCriticalPathPredictor,
          {{features::kLCPCriticalPathAdjustImageLoadPriority.name, "true"},
           {features::kLCPCriticalPathPredictorImageLoadPriority.name,
            kPrioritySetting}}}},
        {});
    auto& properties = *MakeGarbageCollected<TestResourceFetcherProperties>();
    auto* fetcher = CreateFetcher(properties);
    ResourceRequest request(KURL("https://www.example.com/"));

    // Resources for Potentially LCP Elements get a `kExpectedPriority`.
    {
      properties.SetIsOutermostMainFrame(true);
      properties.SetIsSubframeDeprioritizationEnabled(false);
      const auto priority = fetcher->ComputeLoadPriorityForTesting(
          ResourceType::kImage, request, ResourcePriority::kNotVisible,
          FetchParameters::DeferOption::kNoDefer,
          FetchParameters::SpeculativePreloadType::kInDocument,
          RenderBlockingBehavior::kNonBlocking,
          mojom::blink::ScriptType::kClassic, /* is_link_preload=*/false,
          /* resource_width=*/10, /* resource_height=*/10,
          /* is_potentially_lcp_element=*/true);
      EXPECT_EQ(priority, kExpectedPriority)
          << "priority_setting: " << kPrioritySetting;
    }
  }
}

TEST_F(ResourceFetcherTest, Detach) {
  DetachableResourceFetcherProperties& properties =
      MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable();
  auto* const fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties, MakeGarbageCollected<MockFetchContext>(),
                          CreateTaskRunner(), CreateTaskRunner(),
                          MakeGarbageCollected<TestLoaderFactory>(
                              platform_->GetURLLoaderMockFactory()),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));

  EXPECT_EQ(&properties, &fetcher->GetProperties());
  EXPECT_FALSE(properties.IsDetached());

  fetcher->ClearContext();
  // ResourceFetcher::GetProperties always returns the same object.
  EXPECT_EQ(&properties, &fetcher->GetProperties());

  EXPECT_TRUE(properties.IsDetached());
}

TEST_F(ResourceFetcherTest, DuplicatePreloadAllowsPriorityChange) {
  auto* fetcher = CreateFetcher();

  KURL url("http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request =
      FetchParameters::CreateForTest(ResourceRequest(url));
  FetchParameters fetch_params_for_preload =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_preload.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);
  fetch_params_for_preload.SetFetchPriorityHint(
      mojom::blink::FetchPriorityHint::kLow);

  Resource* resource1 =
      MockResource::Fetch(fetch_params_for_preload, fetcher, nullptr);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_EQ(ResourceLoadPriority::kLow,
            resource1->GetResourceRequest().Priority());

  // The second preload fetch returns the first preload but boosts the priority.
  FetchParameters fetch_params_for_preload2 =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params_for_preload2.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);
  fetch_params_for_preload2.SetFetchPriorityHint(
      mojom::blink::FetchPriorityHint::kHigh);
  Resource* resource2 =
      MockResource::Fetch(fetch_params_for_preload2, fetcher, nullptr);

  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsUnusedPreload());
  EXPECT_EQ(resource1, resource2);
  EXPECT_EQ(ResourceLoadPriority::kHigh,
            resource1->GetResourceRequest().Priority());

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // preload matching
  Resource* resource3 =
      MockResource::Fetch(fetch_params_for_request, fetcher, nullptr);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsUnusedPreload());
}

TEST_F(ResourceFetcherTest, StrongReferenceThreshold) {
  // `kTestResourceFilename` has 103 bytes.
  const int64_t kMockResourceSize = 103;

  // Set up the strong reference feature so that the memory cache can keep
  // strong references to `kTestResourcefilename` up to two resources.
  const int64_t kTotalSizeThreshold = kMockResourceSize * 2;
  const int64_t kResourceSizeThreshold = kMockResourceSize;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {features::kMemoryCacheStrongReference,
           {{"memory_cache_strong_ref_total_size_threshold",
             base::NumberToString(kTotalSizeThreshold)},
            {"memory_cache_strong_ref_resource_size_threshold",
             base::NumberToString(kResourceSizeThreshold)}}},
          {features::kResourceFetcherStoresStrongReferences, {}},
      },
      /*disabled_features=*/{});

  ResourceFetcher* fetcher = CreateFetcher();

  // A closure that fetches the given URL with `kTestResourceFilename` and
  // returns whether the memory cache has a strong reference to the resource.
  auto perform_fetch = base::BindLambdaForTesting([&](const KURL& url) {
    ResourceResponse response(url);
    response.SetHttpHeaderField(http_names::kCacheControl,
                                AtomicString("max-age=3600"));
    platform_->GetURLLoaderMockFactory()->RegisterURL(
        url, WrappedResourceResponse(response),
        test::PlatformTestDataPath(kTestResourceFilename));
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    return fetcher->HasStrongReferenceForTesting(resource);
  });

  ASSERT_TRUE(perform_fetch.Run(KURL("http://127.0.0.1:8000/foo.png")));
  ASSERT_TRUE(perform_fetch.Run(KURL("http://127.0.0.1:8000/bar.png")));
  ASSERT_FALSE(perform_fetch.Run(KURL("http://127.0.0.1:8000/baz.png")));
}

TEST_F(ResourceFetcherTest,
       EmulateLoadStartedForInspectorOncePerResourceDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEmulateLoadStartedForInspectorOncePerResource);
  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();

  // Set up the initial fetcher and mark the resource as cached.
  auto* fetcher = CreateFetcher();
  KURL url("http://127.0.0.1:8000/foo.woff2");
  RegisterMockedURLLoad(url);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  resource->SetStatus(ResourceStatus::kCached);

  ASSERT_NE(fetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(fetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));

  // Set up the second fetcher.
  auto* otherContextFetcher = CreateFetcher();
  otherContextFetcher->SetResourceLoadObserver(observer);

  // Ensure that the url is initially not marked as cached or
  // emulated and the observer's last request is empty.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_EQ(observer->GetLastRequest(), std::nullopt);

  otherContextFetcher->EmulateLoadStartedForInspector(
      resource, mojom::blink::RequestContextType::FONT,
      network::mojom::RequestDestination::kFont,
      fetch_initiator_type_names::kCSS);

  // After the first emulation, ensure that the url is not cached,
  // is not marked as emulated and the observer's last
  // request is not empty with the feature disabled.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_NE(observer->GetLastRequest(), std::nullopt);

  // Clear out the last request to start fresh
  observer->ClearLastRequest();

  otherContextFetcher->EmulateLoadStartedForInspector(
      resource, mojom::blink::RequestContextType::FONT,
      network::mojom::RequestDestination::kFont,
      fetch_initiator_type_names::kCSS);

  // After the second emulation, ensure that the url is not cached,
  // the resource is not marked as emulated, and the observer's last
  // request is not empty with the feature disabled. This means that
  // the observer was notified with this emulation.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_NE(observer->GetLastRequest(), std::nullopt);
}

TEST_F(ResourceFetcherTest,
       EmulateLoadStartedForInspectorOncePerResourceEnabled) {
  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();

  // Set up the initial fetcher and mark the resource as cached.
  auto* fetcher = CreateFetcher();
  KURL url("http://127.0.0.1:8000/foo.woff2");
  RegisterMockedURLLoad(url);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  resource->SetStatus(ResourceStatus::kCached);

  ASSERT_NE(fetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(fetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));

  // Set up the second fetcher.
  auto* otherContextFetcher = CreateFetcher();
  otherContextFetcher->SetResourceLoadObserver(observer);

  // Ensure that the url is initially not cached, not marked as emulated,
  // and the observer's last request is empty.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_FALSE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_EQ(observer->GetLastRequest(), std::nullopt);

  otherContextFetcher->EmulateLoadStartedForInspector(
      resource, mojom::blink::RequestContextType::FONT,
      network::mojom::RequestDestination::kFont,
      fetch_initiator_type_names::kCSS);

  // After the first emulation, ensure that the url is not cached,
  // marked as emulated, and the observer's last request is not empty with
  // the feature enabled.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_TRUE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_NE(observer->GetLastRequest(), std::nullopt);

  // Clear out the last request to start fresh
  observer->ClearLastRequest();

  otherContextFetcher->EmulateLoadStartedForInspector(
      resource, mojom::blink::RequestContextType::FONT,
      network::mojom::RequestDestination::kFont,
      fetch_initiator_type_names::kCSS);

  // After the first emulation, ensure that the url is not cached,
  // marked as emulated, and the observer's last request is empty with
  // the feature enabled. This means that the observer was not
  // notified with this emulation.
  ASSERT_EQ(otherContextFetcher->CachedResource(url), nullptr);
  ASSERT_TRUE(
      otherContextFetcher->ResourceHasBeenEmulatedLoadStartedForInspector(url));
  ASSERT_EQ(observer->GetLastRequest(), std::nullopt);
}

class DeferUnusedPreloadResourceFetcherTest : public ResourceFetcherTest {
 public:
  DeferUnusedPreloadResourceFetcherTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kLCPPDeferUnusedPreload,
          {{features::kLcppDeferUnusedPreloadTiming.name, "post_task"}}}},
        {});
  }

  ResourceFetcher* CreateFetcher() {
    MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
    context->SetPotentiallyUnusedPreload(potentially_unused_preloads_);

    scoped_refptr<const SecurityOrigin> source_origin =
        SecurityOrigin::CreateUniqueOpaque();
    auto* fetcher = ResourceFetcherTest::CreateFetcher(
        *MakeGarbageCollected<TestResourceFetcherProperties>(source_origin),
        context);
    fetcher->EnableDeferUnusedPreloadForTesting();
    return fetcher;
  }

  const Vector<KURL>& potentially_unused_preloads() {
    return potentially_unused_preloads_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const Vector<KURL> potentially_unused_preloads_{
      KURL("http://127.0.0.1:8000/test.jpg"),
      KURL("http://127.0.0.1:8000/test2.jpg"),
      KURL("http://127.0.0.1:8000/test3.jpg")};
};

TEST_F(DeferUnusedPreloadResourceFetcherTest, IsPotentiallyUnusedPreload) {
  ResourceFetcher* fetcher = CreateFetcher();
  // A single preload request.
  {
    KURL url = potentially_unused_preloads()[0];
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    fetch_params.SetLinkPreload(/*is_link_preload=*/true);
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);

    platform_->GetURLLoaderMockFactory()->RegisterURL(
        url, WrappedResourceResponse(response),
        test::PlatformTestDataPath(kTestResourceFilename));

    Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
    ASSERT_TRUE(resource);

    // The resource loading is not started yet because it's delayed with the
    // post task. On the other hand, it's stored in the memory cache at this
    // timing.
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_FALSE(resource->IsLoaded());
    EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

    static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
        ->RunUntilIdle();
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_TRUE(resource->IsLoaded());
    EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
  }

  // A preload request is scheduled, then other preload requests to the same
  // resource are passed to the fetcher.
  {
    KURL url = potentially_unused_preloads()[1];
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    fetch_params.SetLinkPreload(/*is_link_preload=*/true);

    ResourceResponse response(url);
    response.SetHttpStatusCode(200);

    platform_->GetURLLoaderMockFactory()->RegisterURL(
        url, WrappedResourceResponse(response),
        test::PlatformTestDataPath(kTestResourceFilename));

    Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
    ASSERT_TRUE(resource);

    // The resource loading is not started yet.
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_FALSE(resource->IsLoaded());
    EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

    // Handle another request with the link preload. This request is not sent,
    // and the resource is the same one as the initial preload resource.
    FetchParameters new_fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    new_fetch_params.SetLinkPreload(/*is_link_preload=*/true);
    Resource* new_resource =
        MockResource::Fetch(new_fetch_params, fetcher, nullptr);
    ASSERT_TRUE(new_resource);
    EXPECT_FALSE(new_resource->IsLoaded());
    ASSERT_EQ(new_resource, resource);

    // Handle another request with the speculative preload. This request is not
    // sent, and the resource is the same one as the initial preload resource.
    FetchParameters another_new_fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    another_new_fetch_params.SetSpeculativePreloadType(
        FetchParameters::SpeculativePreloadType::kInDocument);
    Resource* another_new_resource =
        MockResource::Fetch(another_new_fetch_params, fetcher, nullptr);
    ASSERT_TRUE(another_new_resource);
    EXPECT_FALSE(another_new_resource->IsLoaded());
    ASSERT_EQ(another_new_resource, resource);

    static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
        ->RunUntilIdle();
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_TRUE(new_resource->IsLoaded());
  }

  // A preload request is scheduled, then another non-preload request to the
  // same resource is passed to the fetcher.
  {
    KURL url = potentially_unused_preloads()[2];
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    fetch_params.SetLinkPreload(/*is_link_preload=*/true);

    ResourceResponse response(url);
    response.SetHttpStatusCode(200);

    platform_->GetURLLoaderMockFactory()->RegisterURL(
        url, WrappedResourceResponse(response),
        test::PlatformTestDataPath(kTestResourceFilename));

    Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
    ASSERT_TRUE(resource);

    // The resource loading is not started yet.
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_FALSE(resource->IsLoaded());
    EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

    // Handle another request without preloading signals just like the normal
    // resource request. |resource| is the same one as the initial preload
    // resource, but the request is dispatched immediately.
    FetchParameters new_fetch_params =
        FetchParameters::CreateForTest(ResourceRequest(url));
    Resource* new_resource =
        MockResource::Fetch(new_fetch_params, fetcher, nullptr);
    ASSERT_TRUE(new_resource);
    ASSERT_EQ(new_resource, resource);

    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    EXPECT_TRUE(new_resource->IsLoaded());

    // Confirm if the scheduled task is processed.
    static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
        ->RunUntilIdle();
  }
}

class DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest
    : public DeferUnusedPreloadResourceFetcherTest,
      public testing::WithParamInterface<
          features::LcppDeferUnusedPreloadPreloadedReason> {
 public:
  DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kLCPPDeferUnusedPreload,
          {{features::kLcppDeferUnusedPreloadPreloadedReason.name,
            GetParamString()}}}},
        {});
  }

  features::LcppDeferUnusedPreloadPreloadedReason PreloadedReason() {
    return GetParam();
  }

  std::string GetParamString() {
    switch (PreloadedReason()) {
      case features::LcppDeferUnusedPreloadPreloadedReason::kAll:
        return "all";
      case features::LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly:
        return "link_preload";
      case features::LcppDeferUnusedPreloadPreloadedReason::
          kBrowserSpeculativePreloadOnly:
        return "speculative_preload";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest,
    testing::Values(
        features::LcppDeferUnusedPreloadPreloadedReason::kAll,
        features::LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly,
        features::LcppDeferUnusedPreloadPreloadedReason::
            kBrowserSpeculativePreloadOnly));

TEST_P(DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest, NotPreload) {
  ResourceFetcher* fetcher = CreateFetcher();
  fetcher->SetDeferUnusedPreloadPreloadedReasonForTesting(PreloadedReason());
  KURL url = potentially_unused_preloads()[0];
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));

  fetch_params.SetLinkPreload(/*is_link_preload=*/false);
  ASSERT_FALSE(fetch_params.IsLinkPreload());
  ASSERT_FALSE(fetch_params.IsSpeculativePreload());

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
}

TEST_P(DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest, LinkPreload) {
  ResourceFetcher* fetcher = CreateFetcher();
  fetcher->SetDeferUnusedPreloadPreloadedReasonForTesting(PreloadedReason());
  KURL url = potentially_unused_preloads()[0];
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));

  fetch_params.SetLinkPreload(/*is_link_preload=*/true);
  ASSERT_FALSE(fetch_params.IsSpeculativePreload());

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  switch (PreloadedReason()) {
    case features::LcppDeferUnusedPreloadPreloadedReason::kAll:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadPreloadedReason::
        kBrowserSpeculativePreloadOnly:
      EXPECT_TRUE(resource->IsLoaded());
      break;
  }
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
}

TEST_P(DeferUnusedPreloadWithPreloadedReasonResourceFetcherTest,
       SpeculativePreload) {
  ResourceFetcher* fetcher = CreateFetcher();
  fetcher->SetDeferUnusedPreloadPreloadedReasonForTesting(PreloadedReason());
  KURL url = potentially_unused_preloads()[0];
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));

  fetch_params.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);
  ASSERT_TRUE(fetch_params.IsSpeculativePreload());

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  switch (PreloadedReason()) {
    case features::LcppDeferUnusedPreloadPreloadedReason::kAll:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly:
      EXPECT_TRUE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadPreloadedReason::
        kBrowserSpeculativePreloadOnly:
      EXPECT_FALSE(resource->IsLoaded());
      break;
  }
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
}

class DeferUnusedPreloadWithExcludedResourceTypeResourceFetcherTest
    : public DeferUnusedPreloadResourceFetcherTest,
      public testing::WithParamInterface<
          features::LcppDeferUnusedPreloadExcludedResourceType> {
 public:
  DeferUnusedPreloadWithExcludedResourceTypeResourceFetcherTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kLCPPDeferUnusedPreload,
          {{features::kLcppDeferUnusedPreloadExcludedResourceType.name,
            GetParamString()}}}},
        {});
  }

  features::LcppDeferUnusedPreloadExcludedResourceType ExcludedResourceType() {
    return GetParam();
  }

  std::string GetParamString() {
    switch (ExcludedResourceType()) {
      case features::LcppDeferUnusedPreloadExcludedResourceType::kNone:
        return "none";
      case features::LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet:
        return "stylesheet";
      case features::LcppDeferUnusedPreloadExcludedResourceType::kScript:
        return "script";
      case features::LcppDeferUnusedPreloadExcludedResourceType::kMock:
        return "mock";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DeferUnusedPreloadWithExcludedResourceTypeResourceFetcherTest,
    testing::Values(
        features::LcppDeferUnusedPreloadExcludedResourceType::kNone,
        features::LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet,
        features::LcppDeferUnusedPreloadExcludedResourceType::kScript));

TEST_P(DeferUnusedPreloadWithExcludedResourceTypeResourceFetcherTest,
       LinkPreload) {
  ResourceFetcher* fetcher = CreateFetcher();
  fetcher->SetDeferUnusedPreloadExcludedResourceType(ExcludedResourceType());
  KURL url = potentially_unused_preloads()[0];
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_params.SetLinkPreload(/*is_link_preload=*/true);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  platform_->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response),
      test::PlatformTestDataPath(kTestResourceFilename));

  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);

  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  switch (ExcludedResourceType()) {
    case features::LcppDeferUnusedPreloadExcludedResourceType::kNone:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadExcludedResourceType::kScript:
      EXPECT_FALSE(resource->IsLoaded());
      break;
    case features::LcppDeferUnusedPreloadExcludedResourceType::kMock:
      EXPECT_TRUE(resource->IsLoaded());
      break;
  }

  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));
}

class TransparentPlaceholderResourceFetcherTest
    : public ResourceFetcherTest,
      public testing::WithParamInterface<bool> {
 public:
  TransparentPlaceholderResourceFetcherTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSimplifyLoadingTransparentPlaceholderImage);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSimplifyLoadingTransparentPlaceholderImage);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(TransparentPlaceholderResourceFetcherTest,
                         TransparentPlaceholderResourceFetcherTest,
                         testing::Bool());

TEST_P(TransparentPlaceholderResourceFetcherTest, InspectorAttached) {
  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();
  observer->SetInterestedInAllRequests(true);

  auto* fetcher = CreateFetcher();
  fetcher->SetResourceLoadObserver(observer);
  KURL url(
      "data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
  ResourceRequest request(url);
  request.SetKnownTransparentPlaceholderImageIndex(0);

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(request));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  // Use the presence of |last_request| as an indicator that WillSendRequest()
  // was called, which will happen if the feature is disabled or the inspector
  // is open.
  std::optional<PartialResourceRequest> last_request =
      observer->GetLastRequest();
  EXPECT_TRUE(last_request.has_value());
}

TEST_P(TransparentPlaceholderResourceFetcherTest, InspectorNotAttached) {
  auto* observer = MakeGarbageCollected<TestResourceLoadObserver>();
  observer->SetInterestedInAllRequests(false);

  auto* fetcher = CreateFetcher();
  fetcher->SetResourceLoadObserver(observer);
  KURL url(
      "data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
  ResourceRequest request(url);
  request.SetKnownTransparentPlaceholderImageIndex(0);

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(request));
  Resource* resource = MockResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  // Use the presence of |last_request| as an indicator that WillSendRequest()
  // was called, which will happen if the feature is disabled or the inspector
  // is open.
  std::optional<PartialResourceRequest> last_request =
      observer->GetLastRequest();
  EXPECT_EQ(last_request.has_value(), !GetParam());
}

}  // namespace blink
