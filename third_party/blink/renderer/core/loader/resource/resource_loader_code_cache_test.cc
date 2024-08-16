// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/noop_url_loader.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {
namespace {

class CodeCacheTestLoaderFactory : public ResourceFetcher::LoaderFactory {
 public:
  CodeCacheTestLoaderFactory() = default;
  ~CodeCacheTestLoaderFactory() override = default;

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

class ResourceLoaderCodeCacheTest : public testing::Test {
 protected:
  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

  ResourceFetcher* MakeResourceFetcher(
      TestResourceFetcherProperties* properties,
      FetchContext* context,
      ResourceFetcher::LoaderFactory* loader_factory) {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, CreateTaskRunner(),
        CreateTaskRunner(), loader_factory,
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        /*back_forward_cache_loader_helper=*/nullptr));
  }

  void CommonSetup(v8::Isolate* isolate, const char* url_string = nullptr) {
#if DCHECK_IS_ON()
    WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
    SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
        "codecachewithhashing");

    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    loader_factory_ = MakeGarbageCollected<CodeCacheTestLoaderFactory>();
    auto* fetcher = MakeResourceFetcher(properties, context, loader_factory_);

    KURL url(url_string ? url_string
                        : "codecachewithhashing://www.example.com/");
    ResourceRequest request(url);
    request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);

    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
        kNoCompileHintsProducer = nullptr;
    constexpr v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
        kNoCompileHintsConsumer = nullptr;
    constexpr bool kNoV8CompileHintsMagicCommentRuntimeEnabledFeature = false;
    resource_ = ScriptResource::Fetch(
        params, fetcher, nullptr, isolate, ScriptResource::kNoStreaming,
        kNoCompileHintsProducer, kNoCompileHintsConsumer,
        kNoV8CompileHintsMagicCommentRuntimeEnabledFeature);
    loader_ = resource_->Loader();

    response_ = ResourceResponse(url);
    response_.SetHttpStatusCode(200);
    response_.SetResponseTime(base::Time::Now());
  }

  static const size_t kSha256Bytes = 256 / 8;

  std::vector<uint8_t> MakeSerializedCodeCacheData(base::span<uint8_t> data) {
    const size_t kSerializedDataSize =
        sizeof(CachedMetadataHeader) + data.size();
    std::vector<uint8_t> serialized_data(kSerializedDataSize);
    CachedMetadataHeader* header =
        reinterpret_cast<CachedMetadataHeader*>(&serialized_data[0]);
    header->marker = CachedMetadataHandler::kSingleEntryWithTag;
    header->type = 0;
    memcpy(&serialized_data[sizeof(CachedMetadataHeader)], data.data(),
           data.size());
    return serialized_data;
  }

  std::vector<uint8_t> MakeSerializedCodeCacheDataWithHash(
      base::span<uint8_t> data,
      std::optional<String> source_text = {}) {
    const size_t kSerializedDataSize = sizeof(CachedMetadataHeaderWithHash) +
                                       sizeof(CachedMetadataHeader) +
                                       data.size();
    std::vector<uint8_t> serialized_data(kSerializedDataSize);
    CachedMetadataHeaderWithHash* outer_header =
        reinterpret_cast<CachedMetadataHeaderWithHash*>(&serialized_data[0]);
    outer_header->marker =
        CachedMetadataHandler::kSingleEntryWithHashAndPadding;
    if (source_text.has_value()) {
      std::unique_ptr<ParkableStringImpl::SecureDigest> hash =
          ParkableStringImpl::HashString(source_text->Impl());
      CHECK_EQ(hash->size(), kSha256Bytes);
      memcpy(outer_header->hash, hash->data(), kSha256Bytes);
    }
    CachedMetadataHeader* inner_header =
        reinterpret_cast<CachedMetadataHeader*>(
            &serialized_data[sizeof(CachedMetadataHeaderWithHash)]);
    inner_header->marker = CachedMetadataHandler::kSingleEntryWithTag;
    inner_header->type = 0;
    memcpy(&serialized_data[sizeof(CachedMetadataHeaderWithHash) +
                            sizeof(CachedMetadataHeader)],
           data.data(), data.size());
    return serialized_data;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  // State initialized by CommonSetup().
  Persistent<ScriptResource> resource_;
  Persistent<ResourceLoader> loader_;
  Persistent<CodeCacheTestLoaderFactory> loader_factory_;
  ResourceResponse response_;
};

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyCachedMetadataInfo) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_),
                              /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                              /*cached_metadata=*/std::nullopt);

  // No code cache data was present.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullResponse) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate());

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(MakeSerializedCodeCacheDataWithHash(cache_data)));

  // Code cache data was present.
  EXPECT_EQ(cache_data.size() + sizeof(CachedMetadataHeader),
            resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, CodeCacheFullHttpsScheme) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate(), "https://www.example.com/");

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // Code cache data was present.
  EXPECT_EQ(cache_data.size() + sizeof(CachedMetadataHeader),
            resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, CodeCacheFullHttpsSchemeWithResponseFlag) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate(), "https://www.example.com/");

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  response_.SetShouldUseSourceHashForJSCodeCache(true);
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(MakeSerializedCodeCacheDataWithHash(cache_data)));

  // Code cache data was present.
  EXPECT_EQ(resource_->CodeCacheSize(),
            cache_data.size() + sizeof(CachedMetadataHeader));
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheInvalidOuterType) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate());

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // The serialized metadata was rejected due to an invalid outer type.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheHashCheckSuccess) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate());

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  String source_text("alert('hello world');");

  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(
          MakeSerializedCodeCacheDataWithHash(cache_data, source_text)));

  // Code cache data was present.
  EXPECT_EQ(cache_data.size() + sizeof(CachedMetadataHeader),
            resource_->CodeCacheSize());

  // Successful check.
  resource_->CacheHandler()->Check(loader_factory_->GetCodeCacheHost(),
                                   ParkableString(source_text.Impl()));

  // Now the metadata can be accessed.
  scoped_refptr<CachedMetadata> cached_metadata =
      resource_->CacheHandler()->GetCachedMetadata(0);
  EXPECT_TRUE(cached_metadata.get());
  EXPECT_EQ(cached_metadata->size(), cache_data.size());
  EXPECT_EQ(*(cached_metadata->Data() + 2), cache_data[2]);

  // But trying to load the metadata with the wrong data_type_id fails.
  EXPECT_FALSE(resource_->CacheHandler()->GetCachedMetadata(4));
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheHashCheckFailure) {
  V8TestingScope scope;
  CommonSetup(scope.GetIsolate());

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  String source_text("alert('hello world');");
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(
          MakeSerializedCodeCacheDataWithHash(cache_data, source_text)));

  // Code cache data was present.
  EXPECT_EQ(cache_data.size() + sizeof(CachedMetadataHeader),
            resource_->CodeCacheSize());

  // Failed check: source text is different.
  String source_text_2("alert('improved program');");
  resource_->CacheHandler()->Check(loader_factory_->GetCodeCacheHost(),
                                   ParkableString(source_text_2.Impl()));

  // The metadata has been cleared.
  EXPECT_FALSE(resource_->CodeCacheSize());
  EXPECT_FALSE(resource_->CacheHandler()->GetCachedMetadata(0));
}

class MockTestingPlatformForCodeCache : public TestingPlatformSupport {
 public:
  MockTestingPlatformForCodeCache() = default;
  ~MockTestingPlatformForCodeCache() override = default;

  // TestingPlatformSupport:
  bool ShouldUseCodeCacheWithHashing(const WebURL& request_url) const override {
    return should_use_code_cache_with_hashing_;
  }

  void set_should_use_code_cache_with_hashing(
      bool should_use_code_cache_with_hashing) {
    should_use_code_cache_with_hashing_ = should_use_code_cache_with_hashing;
  }

 private:
  bool should_use_code_cache_with_hashing_ = true;
};

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCachePlatformOverride) {
  ScopedTestingPlatformSupport<MockTestingPlatformForCodeCache> platform;
  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};

  {
    platform->set_should_use_code_cache_with_hashing(true);
    V8TestingScope scope;
    CommonSetup(scope.GetIsolate());
    loader_->DidReceiveResponse(
        WrappedResourceResponse(response_),
        /*body=*/mojo::ScopedDataPipeConsumerHandle(),
        mojo_base::BigBuffer(MakeSerializedCodeCacheDataWithHash(cache_data)));

    // Code cache data was present.
    EXPECT_EQ(resource_->CodeCacheSize(),
              cache_data.size() + sizeof(CachedMetadataHeader));
  }

  {
    platform->set_should_use_code_cache_with_hashing(false);
    V8TestingScope scope;
    CommonSetup(scope.GetIsolate());
    loader_->DidReceiveResponse(
        WrappedResourceResponse(response_),
        /*body=*/mojo::ScopedDataPipeConsumerHandle(),
        mojo_base::BigBuffer(MakeSerializedCodeCacheDataWithHash(cache_data)));

    // Code cache data was absent.
    EXPECT_FALSE(resource_->CodeCacheSize());
    EXPECT_FALSE(resource_->CacheHandler());
  }
}

}  // namespace
}  // namespace blink
