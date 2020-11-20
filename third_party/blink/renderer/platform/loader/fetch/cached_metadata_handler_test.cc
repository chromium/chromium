// Copyright 2020 The Chromium AUthors. All rights reserved.
// Use of this sink code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

class MockPlatform final : public TestingPlatformSupportWithMockScheduler {
 public:
  MockPlatform() = default;
  ~MockPlatform() override = default;

  // From blink::Platform:
  void CacheMetadata(mojom::blink::CodeCacheType cache_type,
                     const WebURL& url,
                     base::Time,
                     const uint8_t*,
                     size_t) override {
    cached_urls_.push_back(url);
  }

  void CacheMetadataInCacheStorage(const WebURL& url,
                                   base::Time,
                                   const uint8_t*,
                                   size_t,
                                   const WebSecurityOrigin&,
                                   const WebString&) override {
    cache_storage_cached_urls_.push_back(url);
  }

  const Vector<WebURL>& CachedURLs() const { return cached_urls_; }
  const Vector<WebURL>& CacheStorageCachedURLs() const {
    return cache_storage_cached_urls_;
  }

 private:
  Vector<WebURL> cached_urls_;
  Vector<WebURL> cache_storage_cached_urls_;
};

ResourceResponse CreateTestResourceResponse() {
  ResourceResponse response(KURL("https://example.com/"));
  response.SetHttpStatusCode(200);
  return response;
}

void SendDataFor(const ResourceResponse& response) {
  constexpr uint8_t kTestData[] = {1, 2, 3, 4, 5};
  std::unique_ptr<CachedMetadataSender> sender = CachedMetadataSender::Create(
      response, mojom::blink::CodeCacheType::kJavascript,
      SecurityOrigin::Create(response.CurrentRequestUrl()));
  sender->Send(kTestData, sizeof(kTestData));
}

TEST(CachedMetadataHandlerTest, SendsMetadataToPlatform) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(CreateTestResourceResponse());

  SendDataFor(response);
  EXPECT_EQ(1u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithSyntheticResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(new Response(...))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);

  SendDataFor(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithPassThroughResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(fetch(evt.request.url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker({response.CurrentRequestUrl()});

  SendDataFor(response);
  EXPECT_EQ(1u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    CachedMetadataHandlerTest,
    DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithDifferentURLResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(fetch(some_different_url))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(
      {KURL("https://example.com/different/url")});

  SendDataFor(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(CachedMetadataHandlerTest,
     SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithCacheResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(cache.match(some_url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetCacheStorageCacheName("dummy");

  SendDataFor(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(1u, mock->CacheStorageCachedURLs().size());
}

}  // namespace
}  // namespace blink
