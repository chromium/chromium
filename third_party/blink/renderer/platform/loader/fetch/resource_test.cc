// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class MockPlatform final : public TestingPlatformSupportWithMockScheduler {
 public:
  MockPlatform() = default;
  ~MockPlatform() override = default;

  // From blink::Platform:
  void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                     const WebURL& url,
                     base::Time,
                     const uint8_t*,
                     size_t) override {
    cached_urls_.push_back(url);
  }

  void CacheMetadataInCacheStorage(const blink::WebURL& url,
                                   base::Time,
                                   const uint8_t*,
                                   size_t,
                                   const blink::WebSecurityOrigin&,
                                   const blink::WebString&) override {
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
  ResourceResponse response(url_test_helpers::ToKURL("https://example.com/"));
  response.SetHttpStatusCode(200);
  return response;
}

void CreateTestResourceAndSetCachedMetadata(const ResourceResponse& response) {
  const uint8_t kTestData[] = {1, 2, 3, 4, 5};
  ResourceRequest request(response.CurrentRequestUrl());
  request.SetRequestorOrigin(
      SecurityOrigin::Create(response.CurrentRequestUrl()));
  auto* resource = MakeGarbageCollected<MockResource>(request);
  resource->SetResponse(response);
  resource->SendCachedMetadata(kTestData, sizeof(kTestData));
  return;
}

}  // anonymous namespace

TEST(ResourceTest, SetCachedMetadata_SendsMetadataToPlatform) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(CreateTestResourceResponse());
  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(1u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithSyntheticResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(new Response(...))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);

  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithPassThroughResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(fetch(evt.request.url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(
      Vector<KURL>(1, response.CurrentRequestUrl()));

  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(1u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorkerWithDifferentURLResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(fetch(some_different_url))
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(Vector<KURL>(
      1, url_test_helpers::ToKURL("https://example.com/different/url")));

  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(0u, mock->CacheStorageCachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_SendsMetadataToPlatformWhenFetchedViaServiceWorkerWithCacheResponse) {
  ScopedTestingPlatformSupport<MockPlatform> mock;

  // Equivalent to service worker calling respondWith(cache.match(some_url));
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  response.SetCacheStorageCacheName("dummy");

  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
  EXPECT_EQ(1u, mock->CacheStorageCachedURLs().size());
}

TEST(ResourceTest, RevalidateWithFragment) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  // Revalidating with a url that differs by only the fragment
  // shouldn't trigger a securiy check.
  url.SetFragmentIdentifier("bar");
  resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidating_response);
}

TEST(ResourceTest, Vary) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  const KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  ResourceRequest new_request(url);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  response.SetHttpHeaderField(http_names::kVary, "*");
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Irrelevant header
  response.SetHttpHeaderField(http_names::kVary, "definitelynotarealheader");
  resource->SetResponse(response);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on new but not old
  new_request.SetHttpHeaderField(http_names::kUserAgent, "something");
  response.SetHttpHeaderField(http_names::kVary, http_names::kUserAgent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));
  new_request.ClearHttpHeaderField(http_names::kUserAgent);

  ResourceRequest old_request(url);
  old_request.SetHttpHeaderField(http_names::kUserAgent, "something");
  old_request.SetHttpHeaderField(http_names::kReferer, "http://foo.com");
  resource = MakeGarbageCollected<MockResource>(old_request);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  // Header present on old but not new
  new_request.ClearHttpHeaderField(http_names::kUserAgent);
  response.SetHttpHeaderField(http_names::kVary, http_names::kUserAgent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on both
  new_request.SetHttpHeaderField(http_names::kUserAgent, "something");
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // One matching, one mismatching
  response.SetHttpHeaderField(http_names::kVary, "User-Agent, Referer");
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Two matching
  new_request.SetHttpHeaderField(http_names::kReferer, "http://foo.com");
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));
}

TEST(ResourceTest, RevalidationFailed) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  MockCacheHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate revalidation start.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidating_response);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_TRUE(resource->CacheHandler());
  EXPECT_NE(original_cache_handler, resource->CacheHandler());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  resource->AppendData(kData, 4);

  EXPECT_FALSE(client->NotifyFinishedCalled());

  resource->FinishForTest();

  EXPECT_TRUE(client->NotifyFinishedCalled());

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
}

TEST(ResourceTest, RevalidationSucceeded) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  MockCacheHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidating_response);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(4u, resource->ResourceBuffer()->size());
  EXPECT_EQ(original_cache_handler, resource->CacheHandler());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededForResourceWithoutBody) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidating_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));
  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededUpdateHeaders) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.AddHttpHeaderField("keep-alive", "keep-alive value");
  response.AddHttpHeaderField("expires", "expires value");
  response.AddHttpHeaderField("last-modified", "last-modified value");
  response.AddHttpHeaderField("proxy-authenticate", "proxy-authenticate value");
  response.AddHttpHeaderField("proxy-connection", "proxy-connection value");
  response.AddHttpHeaderField("x-custom", "custom value");
  resource->ResponseReceived(response);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  // Validate that these headers pre-update.
  EXPECT_EQ("keep-alive value",
            resource->GetResponse().HttpHeaderField("keep-alive"));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField("expires"));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField("last-modified"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-connection value",
            resource->GetResponse().HttpHeaderField("proxy-connection"));
  EXPECT_EQ("custom value",
            resource->GetResponse().HttpHeaderField("x-custom"));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  // Perform a revalidation step.
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  // Headers that aren't copied with an 304 code.
  revalidating_response.AddHttpHeaderField("keep-alive", "garbage");
  revalidating_response.AddHttpHeaderField("expires", "garbage");
  revalidating_response.AddHttpHeaderField("last-modified", "garbage");
  revalidating_response.AddHttpHeaderField("proxy-authenticate", "garbage");
  revalidating_response.AddHttpHeaderField("proxy-connection", "garbage");
  // Header that is updated with 304 code.
  revalidating_response.AddHttpHeaderField("x-custom", "updated");
  resource->ResponseReceived(revalidating_response);

  // Validate the original response.
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());

  // Validate that these headers are not updated.
  EXPECT_EQ("keep-alive value",
            resource->GetResponse().HttpHeaderField("keep-alive"));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField("expires"));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField("last-modified"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-connection value",
            resource->GetResponse().HttpHeaderField("proxy-connection"));
  EXPECT_EQ("updated", resource->GetResponse().HttpHeaderField("x-custom"));

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RedirectDuringRevalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/1");
  const KURL redirect_target_url("http://test.example.com/2");

  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());

  MockCacheHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate a revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));
  EXPECT_TRUE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());
  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHttpHeaderField(
      "location", AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHttpStatusCode(308);
  ResourceRequest redirected_revalidating_request(redirect_target_url);
  resource->WillFollowRedirect(redirected_revalidating_request,
                               redirect_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());
  EXPECT_FALSE(resource->CacheHandler());

  // The final response is received.
  ResourceResponse revalidating_response(redirect_target_url);
  revalidating_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidating_response);

  EXPECT_TRUE(resource->CacheHandler());

  const char kData2[4] = "xyz";
  resource->AppendData(kData2, 3);
  resource->FinishForTest();
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(3u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  EXPECT_TRUE(client->NotifyFinishedCalled());

  // Test the case where a client is added after revalidation is completed.
  Persistent<MockResourceClient> client2 =
      MakeGarbageCollected<MockResourceClient>();
  auto* platform = static_cast<TestingPlatformSupportWithMockScheduler*>(
      Platform::Current());
  resource->AddClient(client2, platform->test_task_runner().get());

  // Because the client is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  platform_->RunUntilIdle();

  EXPECT_TRUE(client2->NotifyFinishedCalled());

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  resource->RemoveClient(client2);
  EXPECT_FALSE(resource->IsAlive());
}

class ScopedResourceMockClock {
 public:
  explicit ScopedResourceMockClock(const base::Clock* clock) {
    Resource::SetClockForTesting(clock);
  }
  ~ScopedResourceMockClock() { Resource::SetClockForTesting(nullptr); }
};

TEST(ResourceTest, StaleWhileRevalidateCacheControl) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ScopedResourceMockClock clock(mock->test_task_runner()->GetMockClock());
  const KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              "max-age=0, stale-while-revalidate=40");

  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_FALSE(resource->ShouldRevalidateStaleResponse());

  mock->AdvanceClockSeconds(1);
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_TRUE(resource->ShouldRevalidateStaleResponse());

  mock->AdvanceClockSeconds(40);
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_TRUE(resource->ShouldRevalidateStaleResponse());
}

TEST(ResourceTest, StaleWhileRevalidateCacheControlWithRedirect) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ScopedResourceMockClock clock(mock->test_task_runner()->GetMockClock());
  const KURL url("http://127.0.0.1:8000/foo.html");
  const KURL redirect_target_url("http://127.0.0.1:8000/food.html");
  ResourceResponse response(url);
  response.SetHttpHeaderField(http_names::kCacheControl, "max-age=50");
  response.SetHttpStatusCode(200);

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHttpHeaderField(
      "location", AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHttpStatusCode(302);
  redirect_response.SetHttpHeaderField(http_names::kCacheControl,
                                       "max-age=0, stale-while-revalidate=40");
  redirect_response.SetAsyncRevalidationRequested(true);
  ResourceRequest redirected_revalidating_request(redirect_target_url);

  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->WillFollowRedirect(redirected_revalidating_request,
                               redirect_response);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_FALSE(resource->ShouldRevalidateStaleResponse());

  mock->AdvanceClockSeconds(41);

  // MustRevalidateDueToCacheHeaders only looks at the stored response not
  // any redirects but ShouldRevalidate and AsyncRevalidationRequest look
  // at the entire redirect chain.
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_TRUE(resource->ShouldRevalidateStaleResponse());
  EXPECT_TRUE(resource->StaleRevalidationRequested());
}

}  // namespace blink
