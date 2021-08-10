// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this sink code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/code_cache_loader_mock.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/noop_web_url_loader.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {
namespace {

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
        scoped_refptr<CodeCacheLoaderMock::Controller> controller)
        : controller_(std::move(controller)) {}
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
      return std::make_unique<CodeCacheLoaderMock>(controller_);
    }

   private:
    scoped_refptr<CodeCacheLoaderMock::Controller> controller_;
  };

  void CommonSetup(const char* url_string = nullptr) {
    SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
        "codecachewithhashing");

    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    controller_ = base::MakeRefCounted<CodeCacheLoaderMock::Controller>();
    controller_->DelayResponse();
    auto* loader_factory =
        MakeGarbageCollected<CodeCacheTestLoaderFactory>(controller_);
    auto* fetcher = MakeResourceFetcher(properties, context, loader_factory);

    KURL url(url_string ? url_string
                        : "codecachewithhashing://www.example.com/");
    ResourceRequest request(url);
    request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);

    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    resource_ = ScriptResource::Fetch(params, fetcher, nullptr,
                                      ScriptResource::kNoStreaming);
    loader_ = resource_->Loader();

    response_ = ResourceResponse(url);
    response_.SetHttpStatusCode(200);
  }

  std::vector<uint8_t> MakeSerializedCodeCacheData() {
    const size_t kCachedMetadataTypeSize = sizeof(uint32_t);
    const size_t kSha256Bytes = 256 / 8;
    const size_t kDataSize =
        kCachedMetadataTypeSize + kSha256Bytes + kCachedMetaDataStart + 1;
    std::vector<uint8_t> data(kDataSize);
    *reinterpret_cast<uint32_t*>(&data[0]) =
        CachedMetadataHandler::kSingleEntryWithHash;
    *reinterpret_cast<uint32_t*>(
        &data[kCachedMetadataTypeSize + kSha256Bytes]) =
        CachedMetadataHandler::kSingleEntry;
    return data;
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  // State initialized by CommonSetup().
  Persistent<ScriptResource> resource_;
  Persistent<ResourceLoader> loader_;
  ResourceResponse response_;
  scoped_refptr<CodeCacheLoaderMock::Controller> controller_;
};

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyResponseFirst) {
  CommonSetup();

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Nothing has changed yet because the code cache hasn't yet responded.
  EXPECT_FALSE(resource_->CodeCacheSize());

  // An empty code cache response means no data was found.
  controller_->Respond(base::Time(), mojo_base::BigBuffer());

  // No code cache data was present.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheEmptyResponseSecond) {
  CommonSetup();

  // An empty code cache response means no data was found.
  controller_->Respond(base::Time(), mojo_base::BigBuffer());

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

  controller_->Respond(base::Time(),
                       mojo_base::BigBuffer(MakeSerializedCodeCacheData()));

  // Code cache data was present.
  EXPECT_TRUE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullResponseSecond) {
  CommonSetup();

  controller_->Respond(base::Time(),
                       mojo_base::BigBuffer(MakeSerializedCodeCacheData()));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Code cache data was present.
  EXPECT_TRUE(resource_->CodeCacheSize());
}

TEST_F(ResourceLoaderCodeCacheTest, WebUICodeCacheFullHttpsScheme) {
  CommonSetup("https://www.example.com/");

  controller_->Respond(base::Time(),
                       mojo_base::BigBuffer(MakeSerializedCodeCacheData()));

  // Nothing has changed yet because the content response hasn't arrived yet.
  EXPECT_FALSE(resource_->CodeCacheSize());

  loader_->DidReceiveResponse(WrappedResourceResponse(response_));

  // Since the URL was https, and the response times were not set, the cached
  // metadata should not be set.
  EXPECT_FALSE(resource_->CodeCacheSize());
}

}  // namespace
}  // namespace blink
