// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

#include <array>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/scheduler/test/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

class ResourceTest : public testing::Test {
 protected:
  test::TaskEnvironmentWithMainThreadScheduler task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
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

// A failed revalidation releases the old body with ClearData(). EncodedSize()
// keeps the response size, but the released bytes stop counting in size().
TEST_F(ResourceTest, ClearDataDropsEncodedMemoryUsage) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kResourceEncodedMemoryAccounting);
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  const KURL url("http://test.example.com/");
  auto* resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);
  const size_t cache_size = MemoryCache::Get()->size();

  resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidating_response);
  ASSERT_FALSE(resource->ResourceBuffer());

  EXPECT_EQ(resource->EncodedSize(), kData.size());
  EXPECT_EQ(resource->size(), resource->OverheadSize());
  EXPECT_EQ(MemoryCache::Get()->size(), cache_size - kData.size());

  // A new body counts again, even with the same size as the released one.
  resource->AppendData(kData);
  EXPECT_EQ(resource->size(), kData.size() + resource->OverheadSize());
  EXPECT_EQ(MemoryCache::Get()->size(), cache_size);
  MemoryCache::Get()->Remove(resource);
}

TEST_F(ResourceTest, RevalidationSucceeded) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
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
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
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
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
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
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
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
  resource->AddClient(client2,
                      task_environment_.GetMainThreadTaskRunner().get());

  // Because the client is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  task_environment_.RunUntilIdle();

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
  ScopedTestingPlatformSupport<TestingPlatformSupport> mock;
  ScopedResourceMockClock clock(task_environment_.GetMockClock());
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

  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_FALSE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_TRUE(resource->ShouldRevalidateStaleResponse());

  task_environment_.AdvanceClock(base::Seconds(40));
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(false));
  EXPECT_TRUE(resource->MustRevalidateDueToCacheHeaders(true));
  EXPECT_TRUE(resource->ShouldRevalidateStaleResponse());
}

TEST_F(ResourceTest, StaleWhileRevalidateCacheControlWithRedirect) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> mock;
  ScopedResourceMockClock clock(task_environment_.GetMockClock());
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

  task_environment_.AdvanceClock(base::Seconds(41));

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
  resource->SetIsAdResource(NoProvenance{});
  EXPECT_TRUE(resource->GetResourceRequest().IsAdResource());
}

TEST_F(ResourceTest, GarbageCollection) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  const KURL url("http://test.example.com/");
  Persistent<MockResource> resource = MakeGarbageCollected<MockResource>(url);
  ResourceResponse response(url);
  resource->ResponseReceived(response);
  resource->FinishForTest();
  MemoryCache::Get()->Add(resource);

  // Add a client.
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  client->SetResource(resource,
                      task_environment_.GetMainThreadTaskRunner().get());

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
  client->SetResource(resource,
                      task_environment_.GetMainThreadTaskRunner().get());

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

// Tests `Resource::CanReuse()` across script worlds, for each world of the
// cached resource, how its response was obtained, and the world of the new
// request.
TEST_F(ResourceTest, CanReuseAcrossWorlds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          kPreventExtensionResourceFetchAcrossIsolatedWorlds,
          kPreventCrossWorldServiceWorkerResourceReuse,
          kPreventAllCrossWorldForCspReuse,
          kPreventAllCrossTargetWorldReuse,
      },
      {});

  // The world of the cached resource or the new request.
  enum class World {
    // A Main World Resource obeying the main world's CSP.
    kMain,

    // A Main World Resource obeying an isolated world's CSP (e.g. a
    // `<link rel=preload>` inserted by a content script).
    kMainIsolatedCsp,

    // Isolated World Resources (e.g. `fetch()` or dynamic `import()` in a
    // content script) of two different isolated worlds.
    kIsolated,
    kIsolatedOther,
  };

  // How the response of the cached resource was obtained.
  enum class ResponseSource {
    // Served from the network server.
    kNetwork,

    // Produced by a ServiceWorker.
    kServiceWorker,

    // The final response was fetched from the network after a network redirect.
    kNetworkAfterRedirect,

    // The redirect was produced by a ServiceWorker, but the final response was
    // fetched from the network, so `WasFetchedViaServiceWorker()` is false.
    kNetworkAfterServiceWorkerRedirect,
  };

  using MatchStatus = Resource::MatchStatus;

  const KURL url("http://127.0.0.1:8000/foo.html");
  const KURL redirect_target_url("http://127.0.0.1:8000/bar.html");

  scoped_refptr<const SecurityOrigin> origin = SecurityOrigin::Create(url);
  DOMWrapperWorld* isolated_world = DOMWrapperWorld::EnsureIsolatedWorld(
      /*isolate=*/nullptr, /*world_id=*/blink::kIsolatedWorldIdLimit - 1);
  DOMWrapperWorld* other_isolated_world = DOMWrapperWorld::EnsureIsolatedWorld(
      /*isolate=*/nullptr, /*world_id=*/blink::kIsolatedWorldIdLimit - 2);

  auto options = [&](World world) -> ResourceLoaderOptions {
    switch (world) {
      case World::kMain:
        return ResourceLoaderOptions(/*world_for_csp=*/nullptr);
      case World::kMainIsolatedCsp:
        return ResourceLoaderOptions(isolated_world);
      case World::kIsolated:
        return ResourceLoaderOptions::CreateForTargetWorld(isolated_world);
      case World::kIsolatedOther:
        return ResourceLoaderOptions::CreateForTargetWorld(
            other_isolated_world);
    }
    NOTREACHED();
  };

  auto create_cached_resource = [&](World cached_world,
                                    ResponseSource cached_response_source) {
    ResourceRequest cached_request(url);
    cached_request.SetRequestorOrigin(origin);
    auto* cached_resource = MakeGarbageCollected<MockResource>(
        cached_request, options(cached_world));

    auto follow_redirect = [&](bool was_fetched_via_service_worker) {
      ResourceResponse redirect_response(url);
      redirect_response.SetHttpStatusCode(302);
      redirect_response.SetWasFetchedViaServiceWorker(
          was_fetched_via_service_worker);
      ResourceRequest redirected_request(redirect_target_url);
      redirected_request.SetRequestorOrigin(origin);
      cached_resource->WillFollowRedirect(redirected_request,
                                          redirect_response);
    };

    ResourceResponse resource_response(url);
    resource_response.SetHttpStatusCode(200);

    switch (cached_response_source) {
      case ResponseSource::kNetwork:
        break;
      case ResponseSource::kServiceWorker:
        resource_response.SetWasFetchedViaServiceWorker(true);
        break;
      case ResponseSource::kNetworkAfterRedirect:
        follow_redirect(/*was_fetched_via_service_worker=*/false);
        resource_response.SetCurrentRequestUrl(redirect_target_url);
        break;
      case ResponseSource::kNetworkAfterServiceWorkerRedirect:
        follow_redirect(/*was_fetched_via_service_worker=*/true);
        resource_response.SetCurrentRequestUrl(redirect_target_url);
        break;
    }
    cached_resource->ResponseReceived(resource_response);
    cached_resource->FinishForTest();
    return cached_resource;
  };

  auto can_reuse = [&](const Resource& cached_resource,
                       World new_request_world) {
    ResourceRequest new_request(url);
    new_request.SetRequestorOrigin(origin);
    FetchParameters new_request_params(std::move(new_request),
                                       options(new_request_world));
    return cached_resource.CanReuse(new_request_params);
  };

  struct TestCase {
    World cached_world;
    ResponseSource cached_response_source;
    World new_request_world;
    MatchStatus expected;
  };
  constexpr auto kCases = std::to_array<TestCase>({
      // A resource from the network (`ResponseSource::kNetwork`):
      // Cannot be reused by a request with a different `WorldForCsp()`, even
      // without other conditions (e.g. Service Worker involvement) that might
      // prevent the reuse.
      {World::kMain, ResponseSource::kNetwork, World::kMain, MatchStatus::kOk},
      {World::kMain, ResponseSource::kNetwork, World::kMainIsolatedCsp,
       MatchStatus::kPreventAllCrossWorldForCspReuse},
      {World::kMain, ResponseSource::kNetwork, World::kIsolated,
       MatchStatus::kPreventAllCrossWorldForCspReuse},
      {World::kMainIsolatedCsp, ResponseSource::kNetwork, World::kMain,
       MatchStatus::kPreventAllCrossWorldForCspReuse},
      {World::kIsolated, ResponseSource::kNetwork, World::kMain,
       MatchStatus::kPreventAllCrossWorldForCspReuse},

      // Resources via a ServiceWorker (`ResponseSource::kServiceWorker`):
      // Cannot be reused by a request with a different `WorldForCsp()`, while a
      // request with the same `WorldForCsp()` can still reuse the entry.
      // `cached_world = World::kIsolated*` are omitted because Isolated World
      // Resources can't be intercepted by ServiceWorkers.
      {World::kMain, ResponseSource::kServiceWorker, World::kMain,
       MatchStatus::kOk},
      {World::kMain, ResponseSource::kServiceWorker, World::kMainIsolatedCsp,
       MatchStatus::kCrossWorldServiceWorkerResourceMismatch},
      {World::kMain, ResponseSource::kServiceWorker, World::kIsolated,
       MatchStatus::kCrossWorldServiceWorkerResourceMismatch},
      {World::kMainIsolatedCsp, ResponseSource::kServiceWorker,
       World::kMainIsolatedCsp, MatchStatus::kOk},

      // The same after a redirect, including a redirect produced by a Service
      // Worker whose final hop fell through to the network
      // (https://crbug.com/551254456).
      {World::kMain, ResponseSource::kNetworkAfterRedirect, World::kMain,
       MatchStatus::kOk},
      {World::kMain, ResponseSource::kNetworkAfterRedirect, World::kIsolated,
       MatchStatus::kPreventAllCrossWorldForCspReuse},
      {World::kMain, ResponseSource::kNetworkAfterServiceWorkerRedirect,
       World::kMain, MatchStatus::kOk},
      {World::kMain, ResponseSource::kNetworkAfterServiceWorkerRedirect,
       World::kIsolated, MatchStatus::kPreventAllCrossWorldForCspReuse},

      // An Isolated World Resource and a Main World Resource are never reused
      // for each other, even when they share the same `WorldForCsp()`, e.g. a
      // `<link rel=preload>` inserted by a content script and a `fetch()` from
      // the same content script. When `WorldForCsp()` matches, the
      // `TargetWorld()` check still triggers `kPreventAllCrossTargetWorldReuse`
      // even for a ServiceWorker response.
      {World::kMainIsolatedCsp, ResponseSource::kNetwork,
       World::kMainIsolatedCsp, MatchStatus::kOk},
      {World::kMainIsolatedCsp, ResponseSource::kNetwork, World::kIsolated,
       MatchStatus::kPreventAllCrossTargetWorldReuse},
      {World::kMainIsolatedCsp, ResponseSource::kServiceWorker,
       World::kIsolated, MatchStatus::kPreventAllCrossTargetWorldReuse},
      {World::kIsolated, ResponseSource::kNetwork, World::kIsolated,
       MatchStatus::kOk},
      {World::kIsolated, ResponseSource::kNetwork, World::kMainIsolatedCsp,
       MatchStatus::kPreventAllCrossTargetWorldReuse},

      // Isolated World Resources of different isolated worlds (e.g. content
      // scripts of different extensions) differ in both `WorldForCsp()` and
      // `TargetWorld()`, and the `WorldForCsp()` check comes first.
      {World::kIsolated, ResponseSource::kNetwork, World::kIsolatedOther,
       MatchStatus::kPreventAllCrossWorldForCspReuse},
  });

  auto world_name = [](World world) -> std::string_view {
    switch (world) {
      case World::kMain:
        return "kMain";
      case World::kMainIsolatedCsp:
        return "kMainIsolatedCsp";
      case World::kIsolated:
        return "kIsolated";
      case World::kIsolatedOther:
        return "kIsolatedOther";
    }
    NOTREACHED();
  };

  auto response_source_name = [](ResponseSource source) -> std::string_view {
    switch (source) {
      case ResponseSource::kNetwork:
        return "kNetwork";
      case ResponseSource::kServiceWorker:
        return "kServiceWorker";
      case ResponseSource::kNetworkAfterRedirect:
        return "kNetworkAfterRedirect";
      case ResponseSource::kNetworkAfterServiceWorkerRedirect:
        return "kNetworkAfterServiceWorkerRedirect";
    }
    NOTREACHED();
  };

  for (size_t i = 0; i < kCases.size(); ++i) {
    const auto& test_case = kCases[i];
    SCOPED_TRACE(testing::Message()
                 << "kCases[" << i
                 << "]: cached_world=" << world_name(test_case.cached_world)
                 << ", cached_response_source="
                 << response_source_name(test_case.cached_response_source)
                 << ", new_request_world="
                 << world_name(test_case.new_request_world));
    MockResource* cached_resource = create_cached_resource(
        test_case.cached_world, test_case.cached_response_source);
    EXPECT_EQ(test_case.expected,
              can_reuse(*cached_resource, test_case.new_request_world));
  }
}

TEST_F(ResourceTest, CanReuseSkipServiceWorker) {
  const KURL url("https://localhost/test.js");
  scoped_refptr<const SecurityOrigin> origin = SecurityOrigin::Create(url);

  ResourceRequest cache_request(url);
  cache_request.SetRequestorOrigin(origin);
  cache_request.SetSkipServiceWorker(false);

  auto* resource = MakeGarbageCollected<MockResource>(cache_request);
  resource->FinishForTest();

  // Same skip_service_worker should match.
  {
    ResourceRequest request(url);
    request.SetRequestorOrigin(origin);
    request.SetSkipServiceWorker(false);
    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    EXPECT_EQ(Resource::MatchStatus::kOk, resource->CanReuse(params));
  }

  // Different skip_service_worker should not match.
  {
    ResourceRequest request(url);
    request.SetRequestorOrigin(origin);
    request.SetSkipServiceWorker(true);
    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    EXPECT_EQ(Resource::MatchStatus::kSkipServiceWorkerDoesNotMatch,
              resource->CanReuse(params));
  }
}

}  // namespace blink
