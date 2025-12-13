// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

class ResourceTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ResourceTest, RevalidateWithFragment) {
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

TEST_F(ResourceTest, Vary) {
  const KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  ResourceRequest new_request(url);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  response.SetHttpHeaderField(http_names::kVary, AtomicString("*"));
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Irrelevant header
  response.SetHttpHeaderField(http_names::kVary,
                              AtomicString("definitelynotarealheader"));
  resource->SetResponse(response);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on new but not old
  new_request.SetHttpHeaderField(http_names::kUserAgent,
                                 AtomicString("something"));
  response.SetHttpHeaderField(http_names::kVary, http_names::kUserAgent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));
  new_request.ClearHttpHeaderField(http_names::kUserAgent);

  ResourceRequest old_request(url);
  old_request.SetHttpHeaderField(http_names::kUserAgent,
                                 AtomicString("something"));
  old_request.SetHttpHeaderField(http_names::kReferer,
                                 AtomicString("http://foo.com"));
  resource = MakeGarbageCollected<MockResource>(old_request);
  resource->ResponseReceived(response);
  resource->FinishForTest();

  // Header present on old but not new
  new_request.ClearHttpHeaderField(http_names::kUserAgent);
  response.SetHttpHeaderField(http_names::kVary, http_names::kUserAgent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on both
  new_request.SetHttpHeaderField(http_names::kUserAgent,
                                 AtomicString("something"));
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // One matching, one mismatching
  response.SetHttpHeaderField(http_names::kVary,
                              AtomicString("User-Agent, Referer"));
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Two matching
  new_request.SetHttpHeaderField(http_names::kReferer,
                                 AtomicString("http://foo.com"));
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));
}

TEST_F(ResourceTest, RevalidationFailed) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Simulate revalidation start.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidating_response);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_FALSE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url));

  resource->AppendData(kData);

  EXPECT_FALSE(client->NotifyFinishedCalled());

  resource->FinishForTest();

  EXPECT_TRUE(client->NotifyFinishedCalled());

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
}

TEST_F(ResourceTest, RevalidationSucceeded) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidating_response);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_TRUE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(4u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url));

  MemoryCache::Get()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST_F(ResourceTest, RevalidationSucceededForResourceWithoutBody) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidating_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_TRUE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url));
  MemoryCache::Get()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST_F(ResourceTest, RevalidationSucceededUpdateHeaders) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.AddHttpHeaderField(AtomicString("keep-alive"),
                              AtomicString("keep-alive value"));
  response.AddHttpHeaderField(http_names::kExpires,
                              AtomicString("expires value"));
  response.AddHttpHeaderField(http_names::kLastModified,
                              AtomicString("last-modified value"));
  response.AddHttpHeaderField(AtomicString("proxy-authenticate"),
                              AtomicString("proxy-authenticate value"));
  response.AddHttpHeaderField(AtomicString("proxy-connection"),
                              AtomicString("proxy-connection value"));
  response.AddHttpHeaderField(AtomicString("x-custom"),
                              AtomicString("custom value"));
  resource->ResponseReceived(response);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  // Validate that these headers pre-update.
  EXPECT_EQ("keep-alive value", resource->GetResponse().HttpHeaderField(
                                    AtomicString("keep-alive")));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField(http_names::kExpires));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField(http_names::kLastModified));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField(
                AtomicString("proxy-authenticate")));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField(
                AtomicString("proxy-authenticate")));
  EXPECT_EQ("proxy-connection value", resource->GetResponse().HttpHeaderField(
                                          AtomicString("proxy-connection")));
  EXPECT_EQ("custom value",
            resource->GetResponse().HttpHeaderField(AtomicString("x-custom")));

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  // Perform a revalidation step.
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(304);
  // Headers that aren't copied with an 304 code.
  revalidating_response.AddHttpHeaderField(AtomicString("keep-alive"),
                                           AtomicString("garbage"));
  revalidating_response.AddHttpHeaderField(http_names::kExpires,
                                           AtomicString("garbage"));
  revalidating_response.AddHttpHeaderField(http_names::kLastModified,
                                           AtomicString("garbage"));
  revalidating_response.AddHttpHeaderField(AtomicString("proxy-authenticate"),
                                           AtomicString("garbage"));
  revalidating_response.AddHttpHeaderField(AtomicString("proxy-connection"),
                                           AtomicString("garbage"));
  // Header that is updated with 304 code.
  revalidating_response.AddHttpHeaderField(AtomicString("x-custom"),
                                           AtomicString("updated"));
  resource->ResponseReceived(revalidating_response);
  EXPECT_TRUE(resource->HasSuccessfulRevalidation());

  // Validate the original response.
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());

  // Validate that these headers are not updated.
  EXPECT_EQ("keep-alive value", resource->GetResponse().HttpHeaderField(
                                    AtomicString("keep-alive")));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField(http_names::kExpires));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField(http_names::kLastModified));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField(
                AtomicString("proxy-authenticate")));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField(
                AtomicString("proxy-authenticate")));
  EXPECT_EQ("proxy-connection value", resource->GetResponse().HttpHeaderField(
                                          AtomicString("proxy-connection")));
  EXPECT_EQ("updated",
            resource->GetResponse().HttpHeaderField(AtomicString("x-custom")));

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST_F(ResourceTest, RedirectDuringRevalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  const KURL url("http://test.example.com/1");
  const KURL redirect_target_url("http://test.example.com/2");

  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  const std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_FALSE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());

  // Simulate a revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));
  EXPECT_TRUE(resource->IsCacheValidator());
  EXPECT_FALSE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());

  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client, nullptr);

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHttpHeaderField(
      http_names::kLocation, AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHttpStatusCode(308);
  ResourceRequest redirected_revalidating_request(redirect_target_url);
  resource->WillFollowRedirect(redirected_revalidating_request,
                               redirect_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_FALSE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());

  // The final response is received.
  ResourceResponse revalidating_response(redirect_target_url);
  revalidating_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidating_response);

  const std::string_view kData2 = "xyz";
  resource->AppendData(kData2);
  resource->FinishForTest();
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_FALSE(resource->HasSuccessfulRevalidation());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(3u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url));

  EXPECT_TRUE(client->NotifyFinishedCalled());

  // Test the case where a client is added after revalidation is completed.
  Persistent<MockResourceClient> client2 =
      MakeGarbageCollected<MockResourceClient>();
  resource->AddClient(client2, platform->test_task_runner().get());

  // Because the client is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  platform->RunUntilIdle();

  EXPECT_TRUE(client2->NotifyFinishedCalled());

  MemoryCache::Get()->Remove(resource);

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

TEST_F(ResourceTest, StaleWhileRevalidateCacheControl) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler> mock;
  ScopedResourceMockClock clock(mock->test_task_runner()->GetMockClock());
  const KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kCacheControl,
      AtomicString("max-age=0, stale-while-revalidate=40"));

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

TEST_F(ResourceTest, StaleWhileRevalidateCacheControlWithRedirect) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler> mock;
  ScopedResourceMockClock clock(mock->test_task_runner()->GetMockClock());
  const KURL url("http://127.0.0.1:8000/foo.html");
  const KURL redirect_target_url("http://127.0.0.1:8000/food.html");
  ResourceResponse response(url);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=50"));
  response.SetHttpStatusCode(200);

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHttpHeaderField(
      http_names::kLocation, AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHttpStatusCode(302);
  redirect_response.SetHttpHeaderField(
      http_names::kCacheControl,
      AtomicString("max-age=0, stale-while-revalidate=40"));
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

TEST_F(ResourceTest, FreshnessLifetime) {
  const KURL url("http://127.0.0.1:8000/foo.html");
  const KURL redirect_target_url("http://127.0.0.1:8000/food.html");
  ResourceResponse response(url);
  response.SetHttpHeaderField(http_names::kCacheControl,
                              AtomicString("max-age=50"));
  response.SetHttpStatusCode(200);

  auto* resource = MakeGarbageCollected<MockResource>(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();
  EXPECT_EQ(resource->FreshnessLifetime(), base::Seconds(50));

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHttpHeaderField(
      http_names::kLocation, AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHttpStatusCode(302);
  redirect_response.SetHttpHeaderField(http_names::kCacheControl,
                                       AtomicString("max-age=10"));
  redirect_response.SetAsyncRevalidationRequested(true);
  ResourceRequest redirected_revalidating_request(redirect_target_url);

  auto* resource_redirected = MakeGarbageCollected<MockResource>(url);
  resource_redirected->WillFollowRedirect(redirected_revalidating_request,
                                          redirect_response);
  resource_redirected->ResponseReceived(response);
  resource_redirected->FinishForTest();

  EXPECT_EQ(resource_redirected->FreshnessLifetime(), base::Seconds(10));
}

// This is a regression test for https://crbug.com/1062837.
TEST_F(ResourceTest, DefaultOverheadSize) {
  const KURL url("http://127.0.0.1:8000/foo.html");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  EXPECT_EQ(resource->CalculateOverheadSizeForTest(), resource->OverheadSize());
}

TEST_F(ResourceTest, SetIsAdResource) {
  const KURL url("http://127.0.0.1:8000/foo.html");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  EXPECT_FALSE(resource->GetResourceRequest().IsAdResource());
  resource->SetIsAdResource();
  EXPECT_TRUE(resource->GetResourceRequest().IsAdResource());
}

TEST_F(ResourceTest, GarbageCollection) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  const KURL url("http://test.example.com/");
  Persistent<MockResource> resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Add a client.
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  client->SetResource(resource, platform->test_task_runner().get());

  EXPECT_TRUE(resource->IsAlive());

  // Garbage collect the client.
  // This shouldn't crash due to checks around GC and prefinalizers.
  WeakPersistent<MockResourceClient> weak_client = client.Get();
  client = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(weak_client);

  // Add a client again.
  client = MakeGarbageCollected<MockResourceClient>();
  client->SetResource(resource, platform->test_task_runner().get());

  EXPECT_TRUE(resource->IsAlive());

  // Garbage collect the client and resource together.
  weak_client = client.Get();
  client = nullptr;
  WeakPersistent<MockResource> weak_resource = resource.Get();
  resource = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  EXPECT_FALSE(weak_client);
  EXPECT_FALSE(weak_resource);
}

}  // namespace blink
