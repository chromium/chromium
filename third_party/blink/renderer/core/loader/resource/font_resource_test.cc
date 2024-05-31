// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/font_resource.h"

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/resource/mock_font_resource_client.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/background_code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_url_loader.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_background_resource_fetch_assets.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_resource_load_info_notifier.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_url_loader_factory_for_background_thread.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class FontResourceTest : public testing::Test {
 public:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 private:
  test::TaskEnvironment task_environment_;
};

class CacheAwareFontResourceTest : public FontResourceTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebFontsCacheAwareTimeoutAdaption);
    FontResourceTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FontResourceStrongReferenceTest : public FontResourceTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kMemoryCacheStrongReference,
         features::kResourceFetcherStoresStrongReferences},
        {});
    FontResourceTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests if ResourceFetcher works fine with FontResource that requires deferred
// loading supports.
TEST_F(FontResourceTest,
       ResourceFetcherRevalidateDeferedResourceFromTwoInitiators) {
  KURL url("http://127.0.0.1:8000/font.woff");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kETag, AtomicString("1234567890"));
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the LoaderFactory.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url, "", WrappedResourceResponse(response));

  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));

  // Fetch to cache a resource.
  ResourceRequest request1(url);
  FetchParameters fetch_params1 =
      FetchParameters::CreateForTest(std::move(request1));
  Resource* resource1 = FontResource::Fetch(fetch_params1, fetcher, nullptr);
  ASSERT_FALSE(resource1->ErrorOccurred());
  fetcher->StartLoad(resource1);
  url_test_helpers::ServeAsynchronousRequests();
  EXPECT_TRUE(resource1->IsLoaded());
  EXPECT_FALSE(resource1->ErrorOccurred());

  // Set the context as it is on reloads.
  properties->SetIsLoadComplete(true);

  // Revalidate the resource.
  ResourceRequest request2(url);
  request2.SetCacheMode(mojom::FetchCacheMode::kValidateCache);
  FetchParameters fetch_params2 =
      FetchParameters::CreateForTest(std::move(request2));
  Resource* resource2 = FontResource::Fetch(fetch_params2, fetcher, nullptr);
  ASSERT_FALSE(resource2->ErrorOccurred());
  EXPECT_EQ(resource1, resource2);
  EXPECT_TRUE(resource2->IsCacheValidator());
  EXPECT_TRUE(resource2->StillNeedsLoad());

  // Fetch the same resource again before actual load operation starts.
  ResourceRequest request3(url);
  request3.SetCacheMode(mojom::FetchCacheMode::kValidateCache);
  FetchParameters fetch_params3 =
      FetchParameters::CreateForTest(std::move(request3));
  Resource* resource3 = FontResource::Fetch(fetch_params3, fetcher, nullptr);
  ASSERT_FALSE(resource3->ErrorOccurred());
  EXPECT_EQ(resource2, resource3);
  EXPECT_TRUE(resource3->IsCacheValidator());
  EXPECT_TRUE(resource3->StillNeedsLoad());

  // StartLoad() can be called from any initiator. Here, call it from the
  // latter.
  fetcher->StartLoad(resource3);
  url_test_helpers::ServeAsynchronousRequests();
  EXPECT_TRUE(resource3->IsLoaded());
  EXPECT_FALSE(resource3->ErrorOccurred());
  EXPECT_TRUE(resource2->IsLoaded());
  EXPECT_FALSE(resource2->ErrorOccurred());

  MemoryCache::Get()->Remove(resource1);
}

// Tests if the RevalidationPolicy UMA works properly for fonts.
TEST_F(FontResourceTest, RevalidationPolicyMetrics) {
  base::HistogramTester histogram_tester;
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));

  KURL url_preload_font("http://127.0.0.1:8000/font_preload.ttf");
  ResourceResponse response_preload_font(url_preload_font);
  response_preload_font.SetHttpStatusCode(200);
  response_preload_font.SetHttpHeaderField(http_names::kCacheControl,
                                           AtomicString("max-age=3600"));
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url_preload_font, "", WrappedResourceResponse(response_preload_font));

  // Test font preloads are immediately loaded.
  FetchParameters fetch_params_preload =
      FetchParameters::CreateForTest(ResourceRequest(url_preload_font));
  fetch_params_preload.SetLinkPreload(true);

  Resource* resource =
      FontResource::Fetch(fetch_params_preload, fetcher, nullptr);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(resource);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  Resource* new_resource =
      FontResource::Fetch(fetch_params_preload, fetcher, nullptr);
  EXPECT_EQ(resource, new_resource);

  // Test histograms.
  histogram_tester.ExpectTotalCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Font", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Font",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kLoad),
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Font",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kUse), 1);

  KURL url_font("http://127.0.0.1:8000/font.ttf");
  ResourceResponse response_font(url_preload_font);
  response_font.SetHttpStatusCode(200);
  response_font.SetHttpHeaderField(http_names::kCacheControl,
                                   AtomicString("max-age=3600"));
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url_font, "", WrappedResourceResponse(response_font));

  // Test deferred and ordinal font loads are correctly counted as deferred.
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url_font));
  resource = FontResource::Fetch(fetch_params, fetcher, nullptr);
  ASSERT_TRUE(resource);
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Font",
                                    1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Font",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kDefer),
      1);
  fetcher->StartLoad(resource);
  url_test_helpers::ServeAsynchronousRequests();
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Font",
                                    2);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Font",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::
                           kPreviouslyDeferredLoad),
      1);
  // Load the resource again, deferred resource already loaded shall be counted
  // as kUse.
  resource = FontResource::Fetch(fetch_params, fetcher, nullptr);
  histogram_tester.ExpectTotalCount("Blink.MemoryCache.RevalidationPolicy.Font",
                                    3);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Font",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kUse), 1);
}

// Tests if cache-aware font loading works correctly.
TEST_F(CacheAwareFontResourceTest, CacheAwareFontLoading) {
  KURL url("http://127.0.0.1:8000/font.woff");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the LoaderFactory.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url, "", WrappedResourceResponse(response));

  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  ResourceFetcher* fetcher = document.Fetcher();
  auto* src_uri_value = MakeGarbageCollected<cssvalue::CSSURIValue>(
      CSSUrlData(AtomicString(url.GetString()), url,
                 Referrer(document.Url(), document.GetReferrerPolicy()),
                 OriginClean::kTrue, false /* is_ad_related */));
  auto* src_value =
      CSSFontFaceSrcValue::Create(src_uri_value, nullptr /* world */);

  // Route font requests in this test through CSSFontFaceSrcValue::Fetch
  // instead of calling FontResource::Fetch directly. CSSFontFaceSrcValue
  // requests a FontResource only once, and skips calling FontResource::Fetch
  // on future CSSFontFaceSrcValue::Fetch calls. This tests wants to ensure
  // correct behavior in the case where we reuse a FontResource without it being
  // a "cache hit" in ResourceFetcher's view.
  Persistent<MockFontResourceClient> client =
      MakeGarbageCollected<MockFontResourceClient>();
  FontResource& resource =
      src_value->Fetch(document.GetExecutionContext(), client);

  fetcher->StartLoad(&resource);
  EXPECT_TRUE(resource.Loader()->IsCacheAwareLoadingActivated());
  resource.load_limit_state_ = FontResource::LoadLimitState::kUnderLimit;

  // FontResource callbacks should be blocked during cache-aware loading.
  resource.FontLoadShortLimitCallback();
  EXPECT_FALSE(client->FontLoadShortLimitExceededCalled());

  // Fail first request as disk cache miss.
  resource.Loader()->HandleError(ResourceError::CacheMissError(url));

  // Once cache miss error returns, previously blocked callbacks should be
  // called immediately.
  EXPECT_FALSE(resource.Loader()->IsCacheAwareLoadingActivated());
  EXPECT_TRUE(client->FontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client->FontLoadLongLimitExceededCalled());

  // Add client now, FontLoadShortLimitExceeded() should be called.
  Persistent<MockFontResourceClient> client2 =
      MakeGarbageCollected<MockFontResourceClient>();
  FontResource& resource2 =
      src_value->Fetch(document.GetExecutionContext(), client2);
  EXPECT_EQ(&resource, &resource2);
  EXPECT_TRUE(client2->FontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client2->FontLoadLongLimitExceededCalled());

  // FontResource callbacks are not blocked now.
  resource.FontLoadLongLimitCallback();
  EXPECT_TRUE(client->FontLoadLongLimitExceededCalled());

  // Add client now, both callbacks should be called.
  Persistent<MockFontResourceClient> client3 =
      MakeGarbageCollected<MockFontResourceClient>();
  FontResource& resource3 =
      src_value->Fetch(document.GetExecutionContext(), client3);
  EXPECT_EQ(&resource, &resource3);
  EXPECT_TRUE(client3->FontLoadShortLimitExceededCalled());
  EXPECT_TRUE(client3->FontLoadLongLimitExceededCalled());

  url_test_helpers::ServeAsynchronousRequests();
  MemoryCache::Get()->Remove(&resource);
}

TEST_F(FontResourceStrongReferenceTest, FontResourceStrongReference) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));

  KURL url_font("http://127.0.0.1:8000/font.ttf");
  ResourceResponse response_font(url_font);
  response_font.SetHttpStatusCode(200);
  response_font.SetHttpHeaderField(http_names::kCacheControl,
                                   AtomicString("max-age=3600"));
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url_font, "", WrappedResourceResponse(response_font));

  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(url_font));
  Resource* resource = FontResource::Fetch(fetch_params, fetcher, nullptr);
  fetcher->StartLoad(resource);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(resource);

  auto strong_referenced_resources = fetcher->MoveResourceStrongReferences();
  ASSERT_EQ(strong_referenced_resources.size(), 1u);

  strong_referenced_resources = fetcher->MoveResourceStrongReferences();
  ASSERT_EQ(strong_referenced_resources.size(), 0u);
}

TEST_F(FontResourceStrongReferenceTest, FollowCacheControl) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties->MakeDetachable(), context,
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          base::MakeRefCounted<scheduler::FakeTaskRunner>(),
                          MakeGarbageCollected<TestLoaderFactory>(),
                          MakeGarbageCollected<MockContextLifecycleNotifier>(),
                          nullptr /* back_forward_cache_loader_helper */));

  KURL url_font_no_store("http://127.0.0.1:8000/font_no_store.ttf");
  ResourceResponse response_font_no_store(url_font_no_store);
  response_font_no_store.SetHttpStatusCode(200);
  response_font_no_store.SetHttpHeaderField(http_names::kCacheControl,
                                            AtomicString("no-cache, no-store"));
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url_font_no_store, "", WrappedResourceResponse(response_font_no_store));

  FetchParameters fetch_params_no_store =
      FetchParameters::CreateForTest(ResourceRequest(url_font_no_store));
  Resource* resource_no_store =
      FontResource::Fetch(fetch_params_no_store, fetcher, nullptr);
  fetcher->StartLoad(resource_no_store);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(resource_no_store);

  auto strong_referenced_resources = fetcher->MoveResourceStrongReferences();
  ASSERT_EQ(strong_referenced_resources.size(), 0u);
}

namespace {

using LoadStartCallback = base::OnceCallback<void(
    mojo::PendingReceiver<network::mojom::URLLoader>,
    mojo::PendingRemote<network::mojom::URLLoaderClient>)>;

class FakeLoaderFactory final : public ResourceFetcher::LoaderFactory {
 public:
  FakeLoaderFactory(
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      LoadStartCallback load_start_callback)
      : unfreezable_task_runner_(std::move(unfreezable_task_runner)),
        background_resource_fetch_assets_(
            base::MakeRefCounted<FakeBackgroundResourceFetchAssets>(
                background_task_runner,
                std::move(load_start_callback))) {}
  ~FakeLoaderFactory() override = default;

  // ResourceFetcher::LoaderFactory implementation:
  std::unique_ptr<URLLoader> CreateURLLoader(
      const network::ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper*,
      const std::optional<base::UnguessableToken>&
          service_worker_race_network_request_token,
      bool is_from_origin_dirty_style_sheet) override {
    return std::make_unique<BackgroundURLLoader>(
        background_resource_fetch_assets_,
        /*cors_exempt_header_list=*/Vector<String>(), unfreezable_task_runner_,
        /*back_forward_cache_loader_helper=*/nullptr,
        /*background_code_cache_host=*/nullptr);
  }
  CodeCacheHost* GetCodeCacheHost() override { return nullptr; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner_;
  scoped_refptr<WebBackgroundResourceFetchAssets>
      background_resource_fetch_assets_;
};

class TestFontResourceClient final
    : public GarbageCollected<TestFontResourceClient>,
      public FontResourceClient {
 public:
  explicit TestFontResourceClient(base::OnceClosure finish_closure)
      : finish_closure_(std::move(finish_closure)) {}

  void NotifyFinished(Resource* resource) override {
    std::move(finish_closure_).Run();
  }

  // Name for debugging, e.g. shown in memory-infra.
  String DebugName() const override { return "TestFontResourceClient"; }

 private:
  bool error_occurred_ = false;
  base::OnceClosure finish_closure_;
};

network::mojom::URLResponseHeadPtr CreateTestResponse() {
  auto response = network::mojom::URLResponseHead::New();
  response->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response->mime_type = "font/woff2";
  return response;
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

mojo::ScopedDataPipeConsumerHandle CreateTestFontDataPipe() {
  std::optional<Vector<char>> font_data =
      test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"));
  std::string font_data_string(base::as_string_view(*font_data));
  return CreateDataPipeConsumerHandleFilledWithString(font_data_string);
}

mojo::ScopedDataPipeConsumerHandle CreateTestTooSmallFontDataPipe() {
  // Use a test data smaller than 4 bytes to force the "file less than 4 bytes"
  // error.
  return CreateDataPipeConsumerHandleFilledWithString("Foo");
}

}  // namespace
class FontResourceBackgroundProcessorTest : public testing::Test {
 public:
  FontResourceBackgroundProcessorTest()
      : url_(String("http://font-test.example.com/foo" +
                    base::NumberToString(url_counter_++))) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackgroundResourceFetch,
          {{"background-font-response-processor", "true"}}}},
        {});
  }
  ~FontResourceBackgroundProcessorTest() override = default;

 protected:
  KURL url_;
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier_;

 private:
  static int url_counter_;
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};
int FontResourceBackgroundProcessorTest::url_counter_ = 0;

TEST_F(FontResourceBackgroundProcessorTest, Basic) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  context->SetResourceLoadInfoNotifier(&fake_resource_load_info_notifier_);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      scheduler::GetSingleThreadTaskRunnerForTesting();
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});

  mojo::PendingReceiver<network::mojom::URLLoader> loader_pending_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderClient>
      loader_client_pending_remote;

  base::RunLoop run_loop_for_request;
  FakeLoaderFactory* fake_loader_factory =
      MakeGarbageCollected<FakeLoaderFactory>(
          task_runner, background_task_runner,
          base::BindLambdaForTesting(
              [&](mojo::PendingReceiver<network::mojom::URLLoader> loader,
                  mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
                loader_pending_receiver = std::move(loader);
                loader_client_pending_remote = std::move(client);
                run_loop_for_request.Quit();
              }));
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, task_runner, task_runner,
      fake_loader_factory, MakeGarbageCollected<MockContextLifecycleNotifier>(),
      /*back_forward_cache_loader_helper=*/nullptr));

  base::RunLoop run_loop;
  TestFontResourceClient* resource_client =
      MakeGarbageCollected<TestFontResourceClient>(run_loop.QuitClosure());

  ResourceRequest request(url_);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(request));
  FontResource* resource =
      FontResource::Fetch(fetch_params, fetcher, resource_client);
  EXPECT_TRUE(resource);
  fetcher->StartLoad(resource);

  run_loop_for_request.Run();
  ASSERT_TRUE(loader_pending_receiver);
  ASSERT_TRUE(loader_client_pending_remote);
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote));
  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestFontDataPipe(),
                                          /*cached_metadata=*/std::nullopt);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
  const FontCustomPlatformData* font_data = resource->GetCustomFontData();
  EXPECT_TRUE(font_data);
  EXPECT_TRUE(resource->OtsParsingMessage().empty());
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
}

TEST_F(FontResourceBackgroundProcessorTest, InvalidFontData) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
  context->SetResourceLoadInfoNotifier(&fake_resource_load_info_notifier_);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      scheduler::GetSingleThreadTaskRunnerForTesting();
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});

  mojo::PendingReceiver<network::mojom::URLLoader> loader_pending_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderClient>
      loader_client_pending_remote;

  base::RunLoop run_loop_for_request;
  FakeLoaderFactory* fake_loader_factory =
      MakeGarbageCollected<FakeLoaderFactory>(
          task_runner, background_task_runner,
          base::BindLambdaForTesting(
              [&](mojo::PendingReceiver<network::mojom::URLLoader> loader,
                  mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
                loader_pending_receiver = std::move(loader);
                loader_client_pending_remote = std::move(client);
                run_loop_for_request.Quit();
              }));
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, task_runner, task_runner,
      fake_loader_factory, MakeGarbageCollected<MockContextLifecycleNotifier>(),
      /*back_forward_cache_loader_helper=*/nullptr));

  base::RunLoop run_loop;
  TestFontResourceClient* resource_client =
      MakeGarbageCollected<TestFontResourceClient>(run_loop.QuitClosure());

  ResourceRequest request(url_);
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(std::move(request));
  FontResource* resource =
      FontResource::Fetch(fetch_params, fetcher, resource_client);
  EXPECT_TRUE(resource);
  fetcher->StartLoad(resource);

  run_loop_for_request.Run();
  ASSERT_TRUE(loader_pending_receiver);
  ASSERT_TRUE(loader_client_pending_remote);
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_remote(
      std::move(loader_client_pending_remote));
  loader_client_remote->OnReceiveResponse(CreateTestResponse(),
                                          CreateTestTooSmallFontDataPipe(),
                                          /*cached_metadata=*/std::nullopt);
  loader_client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
  const FontCustomPlatformData* font_data = resource->GetCustomFontData();
  EXPECT_FALSE(font_data);
  EXPECT_EQ(resource->OtsParsingMessage(), "file less than 4 bytes");
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kDecodeError);
}

}  // namespace blink
