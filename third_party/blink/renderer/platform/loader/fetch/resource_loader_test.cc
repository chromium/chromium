// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/noop_url_loader.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

using ::testing::_;

class MockUseCounter : public GarbageCollected<MockUseCounter>,
                       public UseCounter {
 public:
  MOCK_METHOD1(CountUse, void(mojom::WebFeature));
  MOCK_METHOD1(CountWebDXFeature, void(mojom::blink::WebDXFeature));
  MOCK_METHOD1(CountDeprecation, void(mojom::WebFeature));
};

}  // namespace

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
    std::unique_ptr<URLLoader> CreateURLLoader(
        const network::ResourceRequest& request,
        const ResourceLoaderOptions& options,
        scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
        BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
        const std::optional<base::UnguessableToken>&
            service_worker_race_network_request_token,
        bool is_from_origin_dirty_style_sheet) override {
      return std::make_unique<NoopURLLoader>(std::move(freezable_task_runner));
    }
    CodeCacheHost* GetCodeCacheHost() override { return nullptr; }
  };

  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

  ResourceFetcher* MakeResourceFetcher(
      TestResourceFetcherProperties* properties,
      FetchContext* context) {
    ResourceFetcherInit init(
        properties->MakeDetachable(), context, CreateTaskRunner(),
        CreateTaskRunner(), MakeGarbageCollected<NoopLoaderFactory>(),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        /*back_forward_cache_loader_helper=*/nullptr);
    use_counter_ = MakeGarbageCollected<testing::StrictMock<MockUseCounter>>();
    init.use_counter = MakeGarbageCollected<DetachableUseCounter>(use_counter_);
    return MakeGarbageCollected<ResourceFetcher>(std::move(init));
  }

  MockUseCounter* UseCounter() const { return use_counter_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  Persistent<MockUseCounter> use_counter_;
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

  loader->DidReceiveResponse(WrappedResourceResponse(response),
                             std::move(consumer),
                             /*cached_metadata=*/std::nullopt);
  loader->DidFinishLoading(base::TimeTicks(), 0, 0, 0);

  size_t actually_written_bytes = 0;
  result =
      producer->WriteData(base::byte_span_from_cstring("he"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(actually_written_bytes, 2u);

  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  result =
      producer->WriteData(base::byte_span_from_cstring("llo"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(actually_written_bytes, 3u);

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

  BytesConsumer* body() { return body_.Get(); }

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

namespace {

bool WillFollowRedirect(ResourceLoader* loader, KURL new_url) {
  auto response_head = network::mojom::URLResponseHead::New();
  auto response =
      WebURLResponse::Create(new_url, *response_head,
                             /*report_security_info=*/true, /*request_id=*/1);
  bool has_devtools_request_id = false;
  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  return loader->WillFollowRedirect(
      new_url, net::SiteForCookies(), /*new_referrer=*/String(),
      network::mojom::ReferrerPolicy::kAlways, "GET", response,
      has_devtools_request_id, &removed_headers, modified_headers,
      /*insecure_scheme_was_upgraded=*/false);
}

}  // namespace

TEST_F(ResourceLoaderTest, AuthorizationCrossOriginRedirect) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  KURL url("https://a.test/");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  request.SetHttpHeaderField(http_names::kAuthorization,
                             AtomicString("Basic foo"));

  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  ResourceLoader* loader = resource->Loader();

  // Redirect to the same origin. Expect no UseCounter call.
  {
    KURL new_url("https://a.test/foo");
    ASSERT_TRUE(WillFollowRedirect(loader, new_url));
    ::testing::Mock::VerifyAndClear(UseCounter());
  }

  // Redirect to a cross origin. Expect a single UseCounter call.
  {
    EXPECT_CALL(*UseCounter(),
                CountUse(mojom::WebFeature::kAuthorizationCrossOrigin))
        .Times(1);
    KURL new_url("https://b.test");
    ASSERT_TRUE(WillFollowRedirect(loader, new_url));
    ::testing::Mock::VerifyAndClear(UseCounter());
  }
}

TEST_F(ResourceLoaderTest, CrossOriginRedirect_NoAuthorization) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeResourceFetcher(properties, context);

  KURL url("https://a.test/");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters params = FetchParameters::CreateForTest(std::move(request));
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  ResourceLoader* loader = resource->Loader();

  // Redirect to a cross origin without Authorization header. Expect no
  // UseCounter call.
  KURL new_url("https://b.test");
  ASSERT_TRUE(WillFollowRedirect(loader, new_url));
  ::testing::Mock::VerifyAndClear(UseCounter());
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
    loader->DidReceiveResponse(WrappedResourceResponse(response),
                               /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                               /*cached_metadata=*/std::nullopt);
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

  void ExpectCnameAliasInfoMatching(CnameAliasInfoForTesting info,
                                    ResourceLoader* loader) {
    EXPECT_EQ(loader->cname_alias_info_for_testing_.has_aliases,
              info.has_aliases);

    if (info.has_aliases) {
      EXPECT_EQ(
          loader->cname_alias_info_for_testing_.was_ad_tagged_based_on_alias,
          info.was_ad_tagged_based_on_alias);
      EXPECT_EQ(
          loader->cname_alias_info_for_testing_.was_blocked_based_on_alias,
          info.was_blocked_based_on_alias);
      EXPECT_EQ(loader->cname_alias_info_for_testing_.list_length,
                info.list_length);
      EXPECT_EQ(loader->cname_alias_info_for_testing_.invalid_count,
                info.invalid_count);
      EXPECT_EQ(loader->cname_alias_info_for_testing_.redundant_count,
                info.redundant_count);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
  CnameAliasInfoForTesting info = {.has_aliases = true,
                                   .was_ad_tagged_based_on_alias = true,
                                   .was_blocked_based_on_alias = true,
                                   .list_length = 3,
                                   .invalid_count = 0,
                                   .redundant_count = 0};

  ExpectCnameAliasInfoMatching(info, loader);
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
  CnameAliasInfoForTesting info = {.has_aliases = true,
                                   .was_ad_tagged_based_on_alias = false,
                                   .was_blocked_based_on_alias = true,
                                   .list_length = 3,
                                   .invalid_count = 0,
                                   .redundant_count = 0};

  ExpectCnameAliasInfoMatching(info, loader);
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
  CnameAliasInfoForTesting info = {.has_aliases = true,
                                   .was_ad_tagged_based_on_alias = true,
                                   .was_blocked_based_on_alias = false,
                                   .list_length = 4,
                                   .invalid_count = 1,
                                   .redundant_count = 0};

  ExpectCnameAliasInfoMatching(info, loader);
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
  CnameAliasInfoForTesting info = {.has_aliases = true,
                                   .was_ad_tagged_based_on_alias = false,
                                   .was_blocked_based_on_alias = false,
                                   .list_length = 5,
                                   .invalid_count = 1,
                                   .redundant_count = 1};

  ExpectCnameAliasInfoMatching(info, loader);
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
  CnameAliasInfoForTesting info = {.has_aliases = false};

  ExpectCnameAliasInfoMatching(info, loader);
}

}  // namespace blink
