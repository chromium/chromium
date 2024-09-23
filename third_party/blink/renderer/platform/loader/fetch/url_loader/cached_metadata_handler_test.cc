// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

class MockGeneratedCodeCache {
 public:
  const Vector<KURL>& CachedURLs() const { return cached_urls_; }
  const Vector<KURL>& CacheStorageCachedURLs() const {
    return cache_storage_cached_urls_;
  }

  void CacheMetadata(mojom::CodeCacheType cache_type,
                     const KURL& url,
                     base::Time,
                     const uint8_t*,
                     size_t) {
    cached_urls_.push_back(url);
  }

  void CacheMetadataInCacheStorage(const KURL& url) {
    cache_storage_cached_urls_.push_back(url);
  }

 private:
  Vector<KURL> cached_urls_;
  Vector<KURL> cache_storage_cached_urls_;
};

class CodeCacheHostMockImpl : public mojom::blink::CodeCacheHost {
 public:
  explicit CodeCacheHostMockImpl(MockGeneratedCodeCache* sim) : sim_(sim) {}

 private:
  // CodeCacheHost implementation.
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {
    sim_->CacheMetadata(cache_type, url, expected_response_time, data.data(),
                        data.size());
  }

  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback) override {}
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const KURL& url) override {}

  void DidGenerateCacheableMetadataInCacheStorage(
      const KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const String& cache_storage_cache_name) override {
    sim_->CacheMetadataInCacheStorage(url);
  }

  raw_ptr<MockGeneratedCodeCache> sim_;
};

ResourceResponse CreateTestResourceResponse() {
  ResourceResponse response(KURL("https://example.com/"));
  response.SetHttpStatusCode(200);
  return response;
}

void SendDataFor(const ResourceResponse& response,
                 MockGeneratedCodeCache* disk) {
  constexpr uint8_t kTestData[] = {1, 2, 3, 4, 5};
  std::unique_ptr<CachedMetadataSender> sender = CachedMetadataSender::Create(
      response, mojom::CodeCacheType::kJavascript,
      SecurityOrigin::Create(response.CurrentRequestUrl()));

  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<mojom::blink::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(disk);
  mojo::Remote<mojom::blink::CodeCacheHost> remote;
  mojo::Receiver<mojom::blink::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));
  sender->Send(&code_cache_host, kTestData);

  // Drain the task queue.
  task_environment.RunUntilIdle();
}

TEST(CachedMetadataHandlerTest, SendsMetadataToPlatform) {
  MockGeneratedCodeCache mock_disk_cache;
  ResourceResponse response(CreateTestResourceResponse());

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(1u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithSyntheticResponse) {
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(new Response(...))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithPassThroughResponse) {
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(fetch(evt.request.url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(1u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithDifferentURLResponse) {
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(fetch(some_different_url))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(
      {KURL("https://example.com/different/url")});

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(0u, mock_disk_cache.CacheStorageCachedURLs().size());
}

TEST(CachedMetadataHandlerTest,
     SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithCacheResponse) {
  MockGeneratedCodeCache mock_disk_cache;

  // Equivalent to service worker calling respondWith(cache.match(some_url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetCacheStorageCacheName("dummy");

  SendDataFor(response, &mock_disk_cache);
  EXPECT_EQ(0u, mock_disk_cache.CachedURLs().size());
  EXPECT_EQ(1u, mock_disk_cache.CacheStorageCachedURLs().size());
}

}  // namespace
}  // namespace blink
