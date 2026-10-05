// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/persistent_cache/pending_backend.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

class MockGeneratedCodeCache
    : public network::mojom::blink::CacheStorageSideDataWriter {
 public:
  const Vector<KURL>& CachedURLs() const { return cached_urls_; }
  size_t CacheStorageWriteCount() const { return cache_storage_write_count_; }

  void CacheMetadata(mojom::CodeCacheType cache_type,
                     const KURL& url,
                     base::Time,
                     const uint8_t*,
                     size_t) {
    cached_urls_.push_back(url);
  }

  void BindSideDataWriter(ResourceResponse& response) {
    response.SetServiceWorkerResponseSource(
        network::mojom::FetchResponseSource::kCacheStorage);
    mojo::PendingRemote<network::mojom::blink::CacheStorageSideDataWriter>
        writer_remote;
    receivers_.Add(this, writer_remote.InitWithNewPipeAndPassReceiver());
    response.SetCacheStorageSideDataWriter(
        mojo::SharedRemote<network::mojom::blink::CacheStorageSideDataWriter>(
            std::move(writer_remote)));
  }

  void FlushForTesting() { receivers_.FlushForTesting(); }

  // network::mojom::blink::CacheStorageSideDataWriter:
  void WriteSideData(mojo_base::BigBuffer data) override {
    ++cache_storage_write_count_;
  }

 private:
  Vector<KURL> cached_urls_;
  size_t cache_storage_write_count_ = 0;
  mojo::ReceiverSet<network::mojom::blink::CacheStorageSideDataWriter>
      receivers_;
};

class CodeCacheHostMockImpl : public mojom::blink::CodeCacheHost {
 public:
  explicit CodeCacheHostMockImpl(MockGeneratedCodeCache* sim) : sim_(sim) {}

 private:
  // CodeCacheHost implementation.
  void GetPendingBackend(mojom::blink::CodeCacheType cache_type,
                         GetPendingBackendCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {
    sim_->CacheMetadata(cache_type, url, expected_response_time, data.data(),
                        data.size());
  }
  void DidGenerateSourceKeyedCacheableMetadata(
      const blink::Vector<uint8_t>& script_hash,
      mojo_base::BigBuffer data) override {}
  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback) override {}
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const KURL& url) override {}

  raw_ptr<MockGeneratedCodeCache> sim_;
};

ResourceResponse CreateTestResourceResponse() {
  ResourceResponse response(KURL("https://example.com/"));
  response.SetHttpStatusCode(200);
  return response;
}

class CachedMetadataHandlerTest : public testing::Test {
 protected:
  void SendDataFor(const ResourceResponse& response,
                   MockGeneratedCodeCache* disk) {
    constexpr uint8_t kTestData[] = {1, 2, 3, 4, 5};
    std::unique_ptr<CachedMetadataSender> sender = CachedMetadataSender::Create(
        response, mojom::CodeCacheType::kJavascript,
        SecurityOrigin::Create(response.CurrentRequestUrl()));

    std::unique_ptr<mojom::blink::CodeCacheHost> mojo_code_cache_host =
        std::make_unique<CodeCacheHostMockImpl>(disk);
    mojo::Remote<mojom::blink::CodeCacheHost> remote;
    mojo::Receiver<mojom::blink::CodeCacheHost> receiver(
        mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
    auto code_cache_host = CodeCacheHost::Create(std::move(remote));
    sender->Send(code_cache_host.get(), kTestData);

    // Flush the receiver pipes to process any pending Mojo messages.
    receiver.FlushForTesting();
    disk->FlushForTesting();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(CachedMetadataHandlerTest, SendsMetadataToPlatform) {
  MockGeneratedCodeCache mock_disk_cache;
  ResourceResponse response(CreateTestResourceResponse());

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(1u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithSyntheticResponse) {
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(new Response(...))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(
    CachedMetadataHandlerTest,
    SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithPassThroughResponse) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(fetch(evt.request.url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(1u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithDifferentURLResponse) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(fetch(some_different_url))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(
      {KURL("https://example.com/different/url")});

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(CachedMetadataHandlerTest,
       SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithCacheResponse) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(cache.match(some_url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});
  mock_disk_cache.BindSideDataWriter(response);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(1u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(CachedMetadataHandlerTest,
       DoesNotSendMetadataToPlatformWhenServiceWorkerCodeCacheDisabled) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(false);
  MockGeneratedCodeCache mock_disk_cache;

  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});
  mock_disk_cache.BindSideDataWriter(response);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithSyntheticCacheResponse) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling
  // respondWith(cache.match(synthetic_response));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  mock_disk_cache.BindSideDataWriter(response);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithDifferentURLCacheResponse) {
  ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling
  // respondWith(cache.match(different_url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(
      {KURL("https://example.com/different/url")});
  mock_disk_cache.BindSideDataWriter(response);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageWriteCount());
}

TEST_F(CachedMetadataHandlerTest,
       ShouldUseIsolatedCodeCacheRespectsServiceWorkerCodeCacheFlag) {
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});

  {
    ScopedServiceWorkerCodeCacheForTest scoped_feature(false);
    EXPECT_FALSE(ShouldUseIsolatedCodeCache(
        mojom::blink::RequestContextType::SCRIPT, response));
  }
  {
    ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
    EXPECT_TRUE(ShouldUseIsolatedCodeCache(
        mojom::blink::RequestContextType::SCRIPT, response));
  }
}

}  // namespace
}  // namespace blink
