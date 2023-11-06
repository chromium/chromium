// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
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
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper) override {
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

  void CommonSetup(const char* url_string = nullptr) {
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
    resource_ = ScriptResource::Fetch(
        params, fetcher, nullptr, ScriptResource::kNoStreaming,
        kNoCompileHintsProducer, kNoCompileHintsConsumer);
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
      absl::optional<String> source_text = {}) {
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

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  // State initialized by CommonSetup().
  Persistent<ScriptResource> resource_;
  Persistent<ResourceLoader> loader_;
  Persistent<CodeCacheTestLoaderFactory> loader_factory_;
  ResourceResponse response_;
};

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyCachedMetadataInfo) {
  CommonSetup();

  loader_->DidReceiveResponse(WrappedResourceResponse(response_),
                              /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                              /*cached_metadata=*/absl::nullopt);

  // No code cache data was present.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullResponse) {
  CommonSetup();
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
  CommonSetup("https://www.example.com/");

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
  CommonSetup("https://www.example.com/");

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
  CommonSetup();

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  loader_->DidReceiveResponse(
      WrappedResourceResponse(response_),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // The serialized metadata was rejected due to an invalid outer type.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheHashCheckSuccess) {
  CommonSetup();

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
  CommonSetup();

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

}  // namespace
}  // namespace blink
