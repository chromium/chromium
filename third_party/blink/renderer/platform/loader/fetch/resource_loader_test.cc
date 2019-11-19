// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include <string>
#include <utility>

#include "mojo/public/c/system/data_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ResourceLoaderTest : public testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderTest);

 public:
  enum class From {
    kServiceWorker,
    kNetwork,
  };

  ResourceLoaderTest()
      : foo_url_("https://foo.test"), bar_url_("https://bar.test") {}

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
        scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
      return std::make_unique<NoopWebURLLoader>(std::move(task_runner));
    }
    std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override {
      return Platform::Current()->CreateCodeCacheLoader();
    }
  };

  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

 private:
  class NoopWebURLLoader final : public WebURLLoader {
   public:
    NoopWebURLLoader(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : task_runner_(task_runner) {}
    ~NoopWebURLLoader() override = default;
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
    void LoadAsynchronously(const WebURLRequest&,
                            WebURLLoaderClient*) override {}

    void SetDefersLoading(bool) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED();
    }
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
      return task_runner_;
    }

   private:
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };
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

TEST_F(ResourceLoaderTest, ResponseType) {
  const scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::Create(foo_url_);
  const scoped_refptr<const SecurityOrigin> no_origin = nullptr;
  const KURL same_origin_url = foo_url_;
  const KURL cross_origin_url = bar_url_;

  TestCase cases[] = {
      // Same origin response:
      {same_origin_url, RequestMode::kNoCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},
      {same_origin_url, RequestMode::kCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},

      // Cross origin, no-cors:
      {cross_origin_url, RequestMode::kNoCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kOpaque},

      // Cross origin, cors:
      {cross_origin_url, RequestMode::kCors, From::kNetwork, origin,
       FetchResponseType::kDefault, FetchResponseType::kCors},
      {cross_origin_url, RequestMode::kCors, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kError},

      // From service worker, no-cors:
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kCors, FetchResponseType::kCors},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kDefault},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kOpaque, FetchResponseType::kOpaque},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kOpaqueRedirect, FetchResponseType::kOpaqueRedirect},

      // From service worker, cors:
      {same_origin_url, RequestMode::kCors, From::kServiceWorker, no_origin,
       FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kCors, FetchResponseType::kCors},
      {same_origin_url, RequestMode::kNoCors, From::kServiceWorker, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kDefault},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url.GetString()
                 << ", requets mode: " << test.request_mode
                 << ", from: " << test.from << ", allowed_origin: "
                 << (test.allowed_origin ? test.allowed_origin->ToString()
                                         : String("<no allowed origin>"))
                 << ", original_response_type: "
                 << test.original_response_type);

    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(origin);
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, CreateTaskRunner(),
        MakeGarbageCollected<NoopLoaderFactory>()));
    ResourceRequest request;
    request.SetUrl(test.url);
    request.SetMode(test.request_mode);
    request.SetRequestContext(mojom::RequestContextType::FETCH);

    FetchParameters fetch_parameters(request);
    if (test.request_mode == network::mojom::RequestMode::kCors) {
      fetch_parameters.SetCrossOriginAccessControl(
          origin.get(), network::mojom::CredentialsMode::kOmit);
    }
    Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
    ResourceLoader* loader = resource->Loader();

    ResourceResponse response(test.url);
    response.SetHttpStatusCode(200);
    response.SetType(test.original_response_type);
    response.SetWasFetchedViaServiceWorker(test.from == From::kServiceWorker);
    if (test.allowed_origin) {
      response.SetHttpHeaderField("access-control-allow-origin",
                                  test.allowed_origin->ToAtomicString());
    }
    response.SetType(test.original_response_type);

    loader->DidReceiveResponse(WrappedResourceResponse(response));
    EXPECT_EQ(test.expectation, resource->GetResponse().GetType());
  }
}

TEST_F(ResourceLoaderTest, LoadResponseBody) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));

  KURL url("https://www.example.com/");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);

  FetchParameters params(request);
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

  MojoResult result = CreateDataPipe(&options, &producer, &consumer);
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
    data.Append(span.data(), span.size());
  }
  EXPECT_EQ(data.ToString(), "hello");
}

TEST_F(ResourceLoaderTest, LoadDataURL_AsyncAndNonStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));

  // Fetch a data url.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters params(request);
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get())
      ->RunUntilIdle();

  // The resource has a parsed body.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), span.size());
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

// Helper class which stores a BytesConsumer passed by RawResource and reads the
// bytes when ReadThroughBytesConsumer is called.
class TestRawResourceClient final
    : public GarbageCollected<TestRawResourceClient>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestRawResourceClient);

 public:
  TestRawResourceClient() = default;

  // Implements RawResourceClient.
  void ResponseBodyReceived(Resource* resource,
                            BytesConsumer& bytes_consumer) override {
    body_ = &bytes_consumer;
  }
  String DebugName() const override { return "TestRawResourceClient"; }

  void Trace(Visitor* visitor) override {
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
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url as a stream on response.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  request.SetUseStreamOnResponse(true);
  FetchParameters params(request);
  auto* raw_resource_client = MakeGarbageCollected<TestRawResourceClient>();
  Resource* resource = RawResource::Fetch(params, fetcher, raw_resource_client);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  task_runner->RunUntilIdle();

  // It's still pending because we don't read the body yet.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // Read through the bytes consumer passed back from the ResourceLoader.
  auto* test_reader = MakeGarbageCollected<BytesConsumerTestReader>(
      raw_resource_client->body());
  Vector<char> body;
  BytesConsumer::Result result;
  std::tie(result, body) = test_reader->Run(task_runner);
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
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));

  // Fetch an empty data url.
  KURL url("data:text/html,");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters params(request);
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
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));

  // Fetch a data url synchronously.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters params(request);
  Resource* resource =
      RawResource::FetchSynchronously(params, fetcher, nullptr);

  // The resource has a parsed body.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), span.size());
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

TEST_F(ResourceLoaderTest, LoadDataURL_SyncEmptyData) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));

  // Fetch an empty data url synchronously.
  KURL url("data:text/html,");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters params(request);
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
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  FetchParameters params(request);
  Resource* resource = RawResource::Fetch(params, fetcher, nullptr);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending since it's deferred.
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending since it's deferred again.
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource should still be pending if it's unset and set in a single
  // task.
  fetcher->SetDefersLoading(false);
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);

  // The resource has a parsed body.
  fetcher->SetDefersLoading(false);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kCached);
  scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
  StringBuilder data;
  for (const auto& span : *buffer) {
    data.Append(span.data(), span.size());
  }
  EXPECT_EQ(data.ToString(), "Hello World!");
}

TEST_F(ResourceLoaderTest, LoadDataURL_DefersAsyncAndStream) {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  FetchContext* context = MakeGarbageCollected<MockFetchContext>();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), context, CreateTaskRunner(),
      MakeGarbageCollected<NoopLoaderFactory>()));
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Fetch a data url as a stream on response.
  KURL url("data:text/plain,Hello%20World!");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::RequestContextType::FETCH);
  request.SetUseStreamOnResponse(true);
  FetchParameters params(request);
  auto* raw_resource_client = MakeGarbageCollected<TestRawResourceClient>();
  Resource* resource = RawResource::Fetch(params, fetcher, raw_resource_client);
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();

  // It's still pending because the body should not provided yet.
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  EXPECT_FALSE(raw_resource_client->body());

  // The body should be provided since not deferring now, but it's still pending
  // since we haven't read the body yet.
  fetcher->SetDefersLoading(false);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  EXPECT_TRUE(raw_resource_client->body());

  // The resource should still be pending when it's set to deferred again. No
  // body is provided when deferred.
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  const char* buffer;
  size_t available;
  BytesConsumer::Result result =
      raw_resource_client->body()->BeginRead(&buffer, &available);
  EXPECT_EQ(BytesConsumer::Result::kShouldWait, result);

  // The resource should still be pending if it's unset and set in a single
  // task. No body is provided when deferred.
  fetcher->SetDefersLoading(false);
  fetcher->SetDefersLoading(true);
  task_runner->RunUntilIdle();
  EXPECT_EQ(resource->GetStatus(), ResourceStatus::kPending);
  result = raw_resource_client->body()->BeginRead(&buffer, &available);
  EXPECT_EQ(BytesConsumer::Result::kShouldWait, result);

  // Read through the bytes consumer passed back from the ResourceLoader.
  fetcher->SetDefersLoading(false);
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
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, CreateTaskRunner(),
        MakeGarbageCollected<NoopLoaderFactory>()));
    ResourceRequest request;
    request.SetUrl(foo_url_);
    request.SetRequestContext(mojom::RequestContextType::FETCH);

    FetchParameters fetch_parameters(request);
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

}  // namespace blink
