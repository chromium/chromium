// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/base/features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/code_cache_loader_mock.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/noop_web_url_loader.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const char kCnameAliasHadAliasesHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.HadAliases";
const char kCnameAliasIsInvalidCountHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.InvalidCount";
const char kCnameAliasIsRedundantCountHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.RedundantCount";
const char kCnameAliasListLengthHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.ListLength";
const char kCnameAliasWasAdTaggedHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.WasAdTaggedBasedOnAlias";
const char kCnameAliasWasBlockedHistogram[] =
    "SubresourceFilter.CnameAlias.Renderer.WasBlockedBasedOnAlias";

class ResourceLoaderTest : public testing::Test {
 public:
  enum class From {
    kServiceWorker,
    kNetwork,
  };

  ResourceLoaderTest()
      : foo_url_("https://foo.test"), bar_url_("https://bar.test") {}
  ResourceLoaderTest(const ResourceLoaderTest&) = delete;
  ResourceLoaderTest& operator=(const ResourceLoaderTest&) = delete;

 protected:
  using RequestMode = network::mojom::RequestMode;
  using FetchResponseType = network::mojom::FetchResponseType;

  struct TestCase {
    const KURL url;
    const RequestMode request_mode;
    const From from;
    const scoped_refptr<const SecurityOrigin> allowed_origin;
    const FetchResponseType original_response_type;
    const FetchResponseType expectation;
  };

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL foo_url_;
  const KURL bar_url_;

  class NoopLoaderFactory final : public ResourceFetcher::LoaderFactory {
    std::unique_ptr<WebURLLoader> CreateURLLoader(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
        WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper)
        override {
      return std::make_unique<NoopWebURLLoader>(
          std::move(freezable_task_runner));
    }
    std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override {
      return std::make_unique<CodeCacheLoaderMock>();
    }
  };

  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

  ResourceFetcher* MakeResourceFetcher(
      TestResourceFetcherProperties* properties,
      FetchContext* context) {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, CreateTaskRunner(),
        CreateTaskRunner(), MakeGarbageCollected<NoopLoaderFactory>(),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

std::ostream& operator<<(std::ostream& o, const ResourceLoaderTest::From& f) {
  switch (f) {
    case ResourceLoaderTest::From::kServiceWorker:
      o << "service worker";
      break;
    case ResourceLoaderTest::From::kNetwork:
      o << "network";
      break;
  }
  return o;
}

TEST_F(ResourceLoaderTest, LoadResponseBody) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  KURL url("https://www.example.com/");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  ResourceLoader* loader = resource->Loader();

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 3;

  MojoResult result = CreateDataPipe(&options, producer, consumer);
  ASSERT_EQ(result, MOJO_RESULT_OK);

  loader->DidReceiveResponse(WrappedResourceResponse(response));
  loader->DidStartLoadingResponseBody(std::move(consumer));
  loader->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);

  uint32_t num_bytes = 2;
  result = producer->WriteData("he", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(num_bytes, 2u);

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  num_bytes = 3;
  result = producer->WriteData("llo", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(num_bytes, 3u);

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  producer.reset();
  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), static_cast<wtf_size_t>(span.size()));
  }
  EXPECT_EQ(data.ToString(), "hello");
}

TEST_F(ResourceLoaderTest, LoadDataURL_AsyncAndNonStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  // Fetch a data url.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  // The resource has a parsed body.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), static_cast<wtf_size_t>(span.size()));
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

// Helper class which stores a BytesConsumer passed by RawResource and reads the
// bytes when ReadThroughBytesConsumer is called.
class TestRawResourceClient final
    : public GarbageCollected<TestRawResourceClient>,
      public RawResourceClient {
 public:
  TestRawResourceClient() = default;

  // Implements RawResourceClient.
  void ResponseBodyReceived(Resource* resource,
                            BytesConsumer& bytes_consumer) override {
    body_ = &bytes_consumer;
  }
  String DebugName() const override { return "TestRawResourceClient"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(body_);
    RawResourceClient::Trace(visitor);
  }

  BytesConsumer* body() { return body_; }

 private:
  Member<BytesConsumer> body_;
};

TEST_F(ResourceLoaderTest, LoadDataURL_AsyncAndStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url as a stream on response.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetUseStreamOnResponse(true);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  auto* raw_resource_client = MakeGarbageCollected<TestRawResourceClient>();
  Resource* resource = RawResource::Fetch(params, fetcher, raw_resource_client);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  task_runner->RunUntilIdle();

  // It's still pending because we don't read the body yet.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // Read through the bytes consumer passed back from the ResourceLoader.
  auto* test_reader = MakeGarbageCollected<BytesConsumerTestReader>(
      raw_resource_client->body());
  auto [result, body] = test_reader->Run(task_runner);
  EXPECT_EQ(result, BytesConsumer::Result::kDone);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  EXPECT_EQ(std::string(body.data(), body.size()), "Hello World!");

  // The body is not set to ResourceBuffer since the response body is requested
  // as a stream.
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  EXPECT_FALSE(buffer);
}

TEST_F(ResourceLoaderTest, LoadDataURL_AsyncEmptyData) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  // Fetch an empty data url.
  KURL url("data:text/html,");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  // It successfully finishes, and no buffer is propagated.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  EXPECT_FALSE(buffer);
}

TEST_F(ResourceLoaderTest, LoadDataURL_Sync) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  // Fetch a data url synchronously.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource =
      RawResource::FetchSynchronously(params, fetcher, nullptr);

  // The resource has a parsed body.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), static_cast<wtf_size_t>(span.size()));
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

TEST_F(ResourceLoaderTest, LoadDataURL_SyncEmptyData) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  // Fetch an empty data url synchronously.
  KURL url("data:text/html,");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource =
      RawResource::FetchSynchronously(params, fetcher, nullptr);

  // It successfully finishes, and no buffer is propagated.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  EXPECT_FALSE(buffer);
}

TEST_F(ResourceLoaderTest, LoadDataURL_DefersAsyncAndNonStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending since it's deferred.
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending since it's deferred again.
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending if it's unset and set in a single
  // task.
  fetcher->SetDefersLoading(LoaderFreezeMode::kNone);
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource has a parsed body.
  fetcher->SetDefersLoading(LoaderFreezeMode::kNone);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), static_cast<wtf_size_t>(span.size()));
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

TEST_F(ResourceLoaderTest, LoadDataURL_DefersAsyncAndStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url as a stream on response.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetUseStreamOnResponse(true);
  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  auto* raw_resource_client = MakeGarbageCollected<TestRawResourceClient>();
  Resource* resource = RawResource::Fetch(params, fetcher, raw_resource_client);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();

  // It's still pending because the body should not provided yet.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  EXPECT_FALSE(raw_resource_client->body());

  // The body should be provided since not deferring now, but it's still pending
  // since we haven't read the body yet.
  fetcher->SetDefersLoading(LoaderFreezeMode::kNone);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  EXPECT_TRUE(raw_resource_client->body());

  // The resource should still be pending when it's set to deferred again. No
  // body is provided when deferred.
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  const char* buffer;
  size_t available;
  BytesConsumer::Result result =
      raw_resource_client->body()->BeginRead(&buffer, &available);
  EXPECT_EQ(BytesConsumer::Result::kShouldWait, result);

  // The resource should still be pending if it's unset and set in a single
  // task. No body is provided when deferred.
  fetcher->SetDefersLoading(LoaderFreezeMode::kNone);
  fetcher->SetDefersLoading(LoaderFreezeMode::kStrict);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  result = raw_resource_client->body()->BeginRead(&buffer, &available);
  EXPECT_EQ(BytesConsumer::Result::kShouldWait, result);

  // Read through the bytes consumer passed back from the ResourceLoader.
  fetcher->SetDefersLoading(LoaderFreezeMode::kNone);
  task_runner->RunUntilIdle();
  auto* test_reader = MakeGarbageCollected<BytesConsumerTestReader>(
      raw_resource_client->body());
  Vector<char> body;
  std::tie(result, body) = test_reader->Run(task_runner);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  EXPECT_EQ(std::string(body.data(), body.size()), "Hello World!");

  // The body is not set to ResourceBuffer since the response body is requested
  // as a stream.
  EXPECT_FALSE(resource->ResourceBuffer());
}

class ResourceLoaderIsolatedCodeCacheTest : public ResourceLoaderTest {
 protected:
  bool LoadAndCheckIsolatedCodeCache(ResourceResponse response) {
    const scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::Create(foo_url_);

    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(origin);
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* fetcher = MakeResourceFetcher(properties, context);
    ResourceRequest request;
    request.SetUrl(foo_url_);
    request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

    FetchParameters fetch_parameters =
        FetchParameters::CreateForTest(std::move(request));
    Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
    ResourceLoader* loader = resource->Loader();

    loader->DidReceiveResponse(WrappedResourceResponse(response));
    return loader->should_use_isolated_code_cache_;
  }
};

TEST_F(ResourceLoaderIsolatedCodeCacheTest, ResponseFromNetwork) {
  ResourceResponse response(foo_url_);
  response.SetHttpStatusCode(200);
  EXPECT_EQ(true, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       SyntheticResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       PassThroughResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(Vector<KURL>(1, foo_url_));
  EXPECT_EQ(true, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest,
       DifferentUrlResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(Vector<KURL>(1, bar_url_));
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

TEST_F(ResourceLoaderIsolatedCodeCacheTest, CacheResponseFromServiceWorker) {
  ResourceResponse response(foo_url_);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetCacheStorageCacheName("dummy");
  // The browser does support code cache for cache_storage Responses, but they
  // are loaded via a different mechanism.  So the ResourceLoader code caching
  // value should be false here.
  EXPECT_EQ(false, LoadAndCheckIsolatedCodeCache(response));
}

class ResourceLoaderSubresourceFilterCnameAliasTest
    : public ResourceLoaderTest {
 public:
  ResourceLoaderSubresourceFilterCnameAliasTest() = default;
  ~ResourceLoaderSubresourceFilterCnameAliasTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kSendCnameAliasesToSubresourceFilterFromRenderer);
    ResourceLoaderTest::SetUp();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void SetMockSubresourceFilterBlockLists(Vector<String> blocked_urls,
                                          Vector<String> tagged_urls) {
    blocked_urls_ = blocked_urls;
    tagged_urls_ = tagged_urls;
  }

  Resource* CreateResource(ResourceRequest request) {
    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    auto* fetcher = MakeResourceFetcherWithMockSubresourceFilter();
    return RawResource::Fetch(params, fetcher, nullptr);
  }

  void GiveResponseToLoader(ResourceResponse response, ResourceLoader* loader) {
    CreateMojoDataPipe();
    loader->DidReceiveResponse(WrappedResourceResponse(response));
  }

 protected:
  FetchContext* MakeFetchContextWithMockSubresourceFilter(
      Vector<String> blocked_urls,
      Vector<String> tagged_urls) {
    auto* context = MakeGarbageCollected<MockFetchContext>();
    context->set_blocked_urls(blocked_urls);
    context->set_tagged_urls(tagged_urls);
    return context;
  }

  ResourceFetcher* MakeResourceFetcherWithMockSubresourceFilter() {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context =
        MakeFetchContextWithMockSubresourceFilter(blocked_urls_, tagged_urls_);
    return MakeResourceFetcher(properties, context);
  }

  void CreateMojoDataPipe() {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 3;

    MojoResult result = CreateDataPipe(&options, producer, consumer);
    ASSERT_EQ(result, MOJO_RESULT_OK);
  }

  void ExpectHistogramsMatching(CnameAliasMetricInfo info) {
    histogram_tester()->ExpectUniqueSample(kCnameAliasHadAliasesHistogram,
                                           info.has_aliases, 1);

    if (info.has_aliases) {
      histogram_tester()->ExpectUniqueSample(kCnameAliasWasAdTaggedHistogram,
                                             info.was_ad_tagged_based_on_alias,
                                             1);
      histogram_tester()->ExpectUniqueSample(
          kCnameAliasWasBlockedHistogram, info.was_blocked_based_on_alias, 1);
      histogram_tester()->ExpectUniqueSample(kCnameAliasListLengthHistogram,
                                             info.list_length, 1);
      histogram_tester()->ExpectUniqueSample(kCnameAliasIsInvalidCountHistogram,
                                             info.invalid_count, 1);
      histogram_tester()->ExpectUniqueSample(
          kCnameAliasIsRedundantCountHistogram, info.redundant_count, 1);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  Vector<String> blocked_urls_;
  Vector<String> tagged_urls_;
};

TEST_F(ResourceLoaderSubresourceFilterCnameAliasTest,
       DnsAliasesCheckedBySubresourceFilterDisallowed_TaggedAndBlocked) {
  // Set the blocklists: the first for blocking, the second for ad-tagging.
  Vector<String> blocked_urls = {"https://bad-ad.com/some_path.html"};
  Vector<String> tagged_urls = {"https://ad.com/some_path.html"};
  SetMockSubresourceFilterBlockLists(blocked_urls, tagged_urls);

  // Create the request.
  KURL url("https://www.example.com/some_path.html");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  // Create the resource and loader.
  Resource* resource = CreateResource(std::move(request));
  ResourceLoader* loader = resource->Loader();

  // Create the response.
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  // Set the CNAME aliases.
  Vector<String> aliases({"ad.com", "bad-ad.com", "alias3.com"});
  response.SetDnsAliases(aliases);

  // Give the response to the loader.
  GiveResponseToLoader(response, loader);

  // Test the histograms to verify that the CNAME aliases were detected.
  // Expect that the resource was tagged as a ad, due to first alias.
  // Expect that the resource was blocked, due to second alias.
  CnameAliasMetricInfo info = {.has_aliases = true,
                               .was_ad_tagged_based_on_alias = true,
                               .was_blocked_based_on_alias = true,
                               .list_length = 3,
                               .invalid_count = 0,
                               .redundant_count = 0};

  ExpectHistogramsMatching(info);
}

TEST_F(ResourceLoaderSubresourceFilterCnameAliasTest,
       DnsAliasesCheckedBySubresourceFilterDisallowed_BlockedOnly) {
  // Set the blocklists: the first for blocking, the second for ad-tagging.
  Vector<String> blocked_urls = {"https://bad-ad.com/some_path.html"};
  Vector<String> tagged_urls = {};
  SetMockSubresourceFilterBlockLists(blocked_urls, tagged_urls);

  // Create the request.
  KURL url("https://www.example.com/some_path.html");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  // Create the resource and loader.
  Resource* resource = CreateResource(std::move(request));
  ResourceLoader* loader = resource->Loader();

  // Create the response.
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  // Set the CNAME aliases.
  Vector<String> aliases({"ad.com", "bad-ad.com", "alias3.com"});
  response.SetDnsAliases(aliases);

  // Give the response to the loader.
  GiveResponseToLoader(response, loader);

  // Test the histograms to verify that the CNAME aliases were detected.
  // Expect that the resource was blocked, due to second alias.
  CnameAliasMetricInfo info = {.has_aliases = true,
                               .was_ad_tagged_based_on_alias = false,
                               .was_blocked_based_on_alias = true,
                               .list_length = 3,
                               .invalid_count = 0,
                               .redundant_count = 0};

  ExpectHistogramsMatching(info);
}

TEST_F(ResourceLoaderSubresourceFilterCnameAliasTest,
       DnsAliasesCheckedBySubresourceFilterDisallowed_TaggedOnly) {
  // Set the blocklists: the first for blocking, the second for ad-tagging.
  Vector<String> blocked_urls = {};
  Vector<String> tagged_urls = {"https://bad-ad.com/some_path.html"};
  SetMockSubresourceFilterBlockLists(blocked_urls, tagged_urls);

  // Create the request.
  KURL url("https://www.example.com/some_path.html");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  // Create the resource and loader.
  Resource* resource = CreateResource(std::move(request));
  ResourceLoader* loader = resource->Loader();

  // Create the response.
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  // Set the CNAME aliases.
  Vector<String> aliases({"ad.com", "", "alias3.com", "bad-ad.com"});
  response.SetDnsAliases(aliases);

  // Give the response to the loader.
  GiveResponseToLoader(response, loader);

  // Test the histograms to verify that the CNAME aliases were detected.
  // Expect that the resource was tagged, due to fourth alias.
  // Expect that the invalid empty alias is counted as such.
  CnameAliasMetricInfo info = {.has_aliases = true,
                               .was_ad_tagged_based_on_alias = true,
                               .was_blocked_based_on_alias = false,
                               .list_length = 4,
                               .invalid_count = 1,
                               .redundant_count = 0};

  ExpectHistogramsMatching(info);
}

TEST_F(ResourceLoaderSubresourceFilterCnameAliasTest,
       DnsAliasesCheckedBySubresourceFilterAllowed_NotBlockedOrTagged) {
  // Set the blocklists: the first for blocking, the second for ad-tagging.
  Vector<String> blocked_urls = {};
  Vector<String> tagged_urls = {};
  SetMockSubresourceFilterBlockLists(blocked_urls, tagged_urls);

  // Create the request.
  KURL url("https://www.example.com/some_path.html");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  // Create the resource and loader.
  Resource* resource = CreateResource(std::move(request));
  ResourceLoader* loader = resource->Loader();

  // Create the response.
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  // Set the CNAME aliases.
  Vector<String> aliases(
      {"non-ad.com", "?", "alias3.com", "not-an-ad.com", "www.example.com"});
  response.SetDnsAliases(aliases);

  // Give the response to the loader.
  GiveResponseToLoader(response, loader);

  // Test the histograms to verify that the CNAME aliases were detected.
  // Expect that the resource was neither tagged nor blocked.
  // Expect that the invalid alias is counted as such.
  // Expect that the redundant (i.e. matching the request URL) fifth alias to be
  // counted as such.
  CnameAliasMetricInfo info = {.has_aliases = true,
                               .was_ad_tagged_based_on_alias = false,
                               .was_blocked_based_on_alias = false,
                               .list_length = 5,
                               .invalid_count = 1,
                               .redundant_count = 1};

  ExpectHistogramsMatching(info);
}

TEST_F(ResourceLoaderSubresourceFilterCnameAliasTest,
       DnsAliasesCheckedBySubresourceFilterNoAliases_NoneDetected) {
  // Set the blocklists: the first for blocking, the second for ad-tagging.
  Vector<String> blocked_urls = {};
  Vector<String> tagged_urls = {};
  SetMockSubresourceFilterBlockLists(blocked_urls, tagged_urls);

  // Create the request.
  KURL url("https://www.example.com/some_path.html");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  // Create the resource and loader.
  Resource* resource = CreateResource(std::move(request));
  ResourceLoader* loader = resource->Loader();

  // Create the response.
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  // Set the CNAME aliases.
  Vector<String> aliases;
  response.SetDnsAliases(aliases);

  // Give the response to the loader.
  GiveResponseToLoader(response, loader);

  // Test the histogram to verify that no aliases were detected.
  CnameAliasMetricInfo info = {.has_aliases = false};

  ExpectHistogramsMatching(info);
}

class ResourceLoaderCacheTransparencyTest : public ResourceLoaderTest {
 public:
  ResourceLoaderCacheTransparencyTest() = default;
  ~ResourceLoaderCacheTransparencyTest() override = default;

  void SetUp() override {
    std::string pervasive_payloads_params =
        "1,http://127.0.0.1:4353/pervasive.js,"
        "2478392C652868C0AAF0316A28284610DBDACF02D66A00B39F3BA75D887F4829";
    feature_list_.InitWithFeaturesAndParameters(
        {{network::features::kPervasivePayloadsList,
          {{"pervasive-payloads", pervasive_payloads_params}}},
         {network::features::kCacheTransparency, {}},
         {net::features::kSplitCacheByNetworkIsolationKey, {}}},
        {/* disabled_features */});

    ResourceLoaderTest::SetUp();
  }

  const ukm::TestUkmRecorder* GetUkmRecorder() {
    return scoped_fake_ukm_recorder_.recorder();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
};

TEST_F(ResourceLoaderCacheTransparencyTest, PervasivePayloadRequested) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  KURL url("http://127.0.0.1:4353/pervasive.js");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  ResourceLoader* loader = resource->Loader();

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  loader->DidReceiveResponse(WrappedResourceResponse(response));
  loader->DidFinishLoading(base::TimeTicks(),
                           /*encoded_data_length=*/0,
                           /*encoded_body_length=*/0,
                           /*decoded_body_length=*/0,
                           /*should_report_corb_blocking=*/false,
                           /*pervasive_payload_requested=*/true);

  base::RunLoop().RunUntilIdle();

  // Check UKM recording
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Network_CacheTransparency::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Network_CacheTransparency::kFoundPervasivePayloadName,
      true);
}

TEST_F(ResourceLoaderCacheTransparencyTest, PervasivePayloadNotRequested) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  KURL url("http://127.0.0.1:4353/cacheable.js");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  ResourceLoader* loader = resource->Loader();

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  loader->DidReceiveResponse(WrappedResourceResponse(response));
  loader->DidFinishLoading(base::TimeTicks(),
                           /*encoded_data_length=*/0,
                           /*encoded_body_length=*/0,
                           /*decoded_body_length=*/0,
                           /*should_report_corb_blocking=*/false,
                           /*pervasive_payload_requested=*/false);

  base::RunLoop().RunUntilIdle();

  // Check UKM recording
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Network_CacheTransparency::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Network_CacheTransparency::kFoundPervasivePayloadName,
      false);
}

}  // namespace blink
