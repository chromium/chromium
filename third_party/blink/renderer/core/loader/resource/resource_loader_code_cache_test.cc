// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
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

using FetchCachedCodeCallback =
    mojom::blink::CodeCacheHost::FetchCachedCodeCallback;
using ProcessCodeCacheRequestCallback =
    base::RepeatingCallback<void(FetchCachedCodeCallback)>;

class DummyCodeCacheHost final : public mojom::blink::CodeCacheHost {
 public:
  explicit DummyCodeCacheHost(
      ProcessCodeCacheRequestCallback process_code_cache_request_callback)
      : process_code_cache_request_callback_(
            std::move(process_code_cache_request_callback)) {}

  // mojom::blink::CodeCacheHost implementations
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {}
  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback callback) override {
    process_code_cache_request_callback_.Run(std::move(callback));
  }
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const KURL& url) override {}
  void DidGenerateCacheableMetadataInCacheStorage(
      const KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const WTF::String& cache_storage_cache_name) override {}

 private:
  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
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
        nullptr /* back_forward_cache_loader_helper */));
  }

  class CodeCacheTestLoaderFactory : public ResourceFetcher::LoaderFactory {
   public:
    explicit CodeCacheTestLoaderFactory(
        ProcessCodeCacheRequestCallback process_code_cache_request_callback) {
      mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<DummyCodeCacheHost>(
              std::move(process_code_cache_request_callback)),
          pending_remote.InitWithNewPipeAndPassReceiver());
      code_cache_host_ = std::make_unique<CodeCacheHost>(
          mojo::Remote<mojom::blink::CodeCacheHost>(std::move(pending_remote)));
    }

    std::unique_ptr<URLLoader> CreateURLLoader(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
        BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
        override {
      return std::make_unique<NoopURLLoader>(std::move(freezable_task_runner));
    }
    CodeCacheHost* GetCodeCacheHost() override {
      return code_cache_host_.get();
    }

   private:
    std::unique_ptr<CodeCacheHost> code_cache_host_;
  };

  void CommonSetup(const char* url_string = nullptr) {
#if DCHECK_IS_ON()
    WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
    SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
        "codecachewithhashing");

    base::RunLoop run_loop;
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* loader_factory = MakeGarbageCollected<CodeCacheTestLoaderFactory>(
        base::BindLambdaForTesting([&](FetchCachedCodeCallback callback) {
          code_cache_response_callback_ = std::move(callback);
          run_loop.Quit();
        }));
    auto* fetcher = MakeResourceFetcher(properties, context, loader_factory);

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
    run_loop.Run();
    loader_ = resource_->Loader();

    response_ = ResourceResponse(url);
    response_.SetHttpStatusCode(200);
  }

  static const size_t kSha256Bytes = 256 / 8;

  std::vector<uint8_t> MakeSerializedCodeCacheData(
      base::span<uint8_t> data,
      absl::optional<String> source_text = {},
      uint32_t data_type_id = 0,
      CachedMetadataHandler::CachedMetadataType outer_type =
          CachedMetadataHandler::kSingleEntryWithHashAndPadding,
      CachedMetadataHandler::CachedMetadataType inner_type =
          CachedMetadataHandler::kSingleEntryWithTag) {
    const size_t kSerializedDataSize = sizeof(CachedMetadataHeaderWithHash) +
                                       sizeof(CachedMetadataHeader) +
                                       data.size();
    std::vector<uint8_t> serialized_data(kSerializedDataSize);
    CachedMetadataHeaderWithHash* outer_header =
        reinterpret_cast<CachedMetadataHeaderWithHash*>(&serialized_data[0]);
    outer_header->marker = outer_type;
    if (source_text.has_value()) {
      std::unique_ptr<ParkableStringImpl::SecureDigest> hash =
          ParkableStringImpl::HashString(source_text->Impl());
      CHECK_EQ(hash->size(), kSha256Bytes);
      memcpy(outer_header->hash, hash->data(), kSha256Bytes);
    }
    CachedMetadataHeader* inner_header =
        reinterpret_cast<CachedMetadataHeader*>(
            &serialized_data[sizeof(CachedMetadataHeaderWithHash)]);
    inner_header->marker = inner_type;
    inner_header->type = data_type_id;
    memcpy(&serialized_data[sizeof(CachedMetadataHeaderWithHash) +
                            sizeof(CachedMetadataHeader)],
           data.data(), data.size());
    return serialized_data;
  }

  void Respond(base::Time time, mojo_base::BigBuffer data) {
    std::move(code_cache_response_callback_).Run(time, std::move(data));
    test::RunPendingTasks();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  // State initialized by CommonSetup().
  Persistent<ScriptResource> resource_;
  Persistent<ResourceLoader> loader_;
  ResourceResponse response_;
  FetchCachedCodeCallback code_cache_response_callback_;
};

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyResponseFirst) {
  CommonSetup();

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Nothing has changed yet because the code cache hasn't yet responded.
  EXPECT_FALSE(resource_->CodeCacheSize());

  // An empty code cache response means no data was found.
  Respond(base::Time(), mojo_base::BigBuffer());

  // No code cache data was present.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyResponseSecond) {
  CommonSetup();

  // An empty code cache response means no data was found.
  Respond(base::Time(), mojo_base::BigBuffer());

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // No code cache data was present.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullResponseFirst) {
  CommonSetup();

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Nothing has changed yet because the code cache hasn't yet responded.
  EXPECT_FALSE(resource_->CodeCacheSize());

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  Respond(base::Time(),
          mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // Code cache data was present.
  EXPECT_EQ(resource_->CodeCacheSize(),
            cache_data.size() + sizeof(CachedMetadataHeader));
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullResponseSecond) {
  CommonSetup();

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  Respond(base::Time(),
          mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Code cache data was present.
  EXPECT_EQ(resource_->CodeCacheSize(),
            cache_data.size() + sizeof(CachedMetadataHeader));
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullHttpsScheme) {
  CommonSetup("https://www.example.com/");

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  Respond(base::Time(),
          mojo_base::BigBuffer(MakeSerializedCodeCacheData(cache_data)));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Since the URL was https, and the response times were not set, the cached
  // metadata should not be set.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheInvalidOuterType) {
  CommonSetup();

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  Respond(base::Time(),
          mojo_base::BigBuffer(MakeSerializedCodeCacheData(
              cache_data, {}, 0, CachedMetadataHandler::kSingleEntryWithTag)));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // The serialized metadata was rejected due to an invalid outer type.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheHashCheckSuccess) {
  CommonSetup();

  std::vector<uint8_t> cache_data{2, 3, 4, 5, 6};
  String source_text("alert('hello world');");
  Respond(base::Time(), mojo_base::BigBuffer(MakeSerializedCodeCacheData(
                            cache_data, source_text)));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Code cache data was present.
  EXPECT_EQ(resource_->CodeCacheSize(),
            cache_data.size() + sizeof(CachedMetadataHeader));

  // Make sure the following steps don't try to do anything too fancy.
  resource_->CacheHandler()->DisableSendToPlatformForTesting();

  // Successful check.
  resource_->CacheHandler()->Check(nullptr, ParkableString(source_text.Impl()));

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
  Respond(base::Time(), mojo_base::BigBuffer(MakeSerializedCodeCacheData(
                            cache_data, source_text)));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Code cache data was present.
  EXPECT_EQ(resource_->CodeCacheSize(),
            cache_data.size() + sizeof(CachedMetadataHeader));

  // Make sure the following steps don't try to do anything too fancy.
  resource_->CacheHandler()->DisableSendToPlatformForTesting();

  // Failed check: source text is different.
  String source_text_2("alert('improved program');");
  resource_->CacheHandler()->Check(nullptr,
                                   ParkableString(source_text_2.Impl()));

  // The metadata has been cleared.
  EXPECT_FALSE(resource_->CodeCacheSize());
  EXPECT_FALSE(resource_->CacheHandler()->GetCachedMetadata(0));
}

}  // namespace
}  // namespace blink
