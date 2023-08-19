/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

// An URL for the original request.
constexpr char kResourceURL[] = "http://resource.com/";

// The origin time of our first request.
constexpr char kOriginalRequestDateAsString[] = "Thu, 25 May 1977 18:30:00 GMT";
constexpr char kOneDayBeforeOriginalRequest[] = "Wed, 24 May 1977 18:30:00 GMT";
constexpr char kOneDayAfterOriginalRequest[] = "Fri, 26 May 1977 18:30:00 GMT";

constexpr base::TimeDelta kOneDay = base::Days(1);

}  // namespace

class MemoryCacheCorrectnessTest : public testing::Test {
 protected:
  MockResource* ResourceFromResourceResponse(ResourceResponse response) {
    if (response.CurrentRequestUrl().IsNull())
      response.SetCurrentRequestUrl(KURL(kResourceURL));
    ResourceRequest request(response.CurrentRequestUrl());
    request.SetRequestorOrigin(GetSecurityOrigin());
    auto* resource = MakeGarbageCollected<MockResource>(request);
    resource->SetResponse(response);
    resource->FinishForTest();
    AddResourceToMemoryCache(resource);

    return resource;
  }
  MockResource* ResourceFromResourceRequest(ResourceRequest request) {
    if (request.Url().IsNull())
      request.SetUrl(KURL(kResourceURL));
    auto* resource = MakeGarbageCollected<MockResource>(request);
    ResourceResponse response(KURL{kResourceURL});
    response.SetMimeType(AtomicString("text/html"));
    resource->SetResponse(response);
    resource->FinishForTest();
    AddResourceToMemoryCache(resource);

    return resource;
  }
  void AddResourceToMemoryCache(Resource* resource) {
    MemoryCache::Get()->Add(resource);
  }
  // TODO(toyoshim): Consider to use MockResource for all tests instead of
  // RawResource.
  RawResource* FetchRawResource() {
    ResourceRequest resource_request{KURL(kResourceURL)};
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::INTERNAL);
    resource_request.SetRequestorOrigin(GetSecurityOrigin());
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(resource_request));
    return RawResource::Fetch(fetch_params, Fetcher(), nullptr);
  }
  MockResource* FetchMockResource() {
    ResourceRequest resource_request{KURL(kResourceURL)};
    resource_request.SetRequestorOrigin(GetSecurityOrigin());
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(resource_request));
    return MockResource::Fetch(fetch_params, Fetcher(), nullptr);
  }
  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }
  void AdvanceClock(base::TimeDelta delta) { platform_->AdvanceClock(delta); }
  scoped_refptr<const SecurityOrigin> GetSecurityOrigin() const {
    return security_origin_;
  }

 private:
  // Overrides testing::Test.
  void SetUp() override {
    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ = ReplaceMemoryCacheForTesting(
        MakeGarbageCollected<MemoryCache>(platform_->test_task_runner()));

    security_origin_ = SecurityOrigin::CreateUniqueOpaque();
    MockFetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* properties =
        MakeGarbageCollected<TestResourceFetcherProperties>(security_origin_);
    properties->SetShouldBlockLoadingSubResource(true);
    fetcher_ = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context,
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>(),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
    Resource::SetClockForTesting(platform_->test_task_runner()->GetMockClock());
  }
  void TearDown() override {
    MemoryCache::Get()->EvictResources();

    Resource::SetClockForTesting(nullptr);

    // Yield the ownership of the global memory cache back.
    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  Persistent<MemoryCache> global_memory_cache_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  Persistent<ResourceFetcher> fetcher_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(MemoryCacheCorrectnessTest, FreshFromLastModified) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kLastModified, AtomicString(kOneDayBeforeOriginalRequest));

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the implicit freshness period of this resource
  // before we make a request.
  AdvanceClock(base::Seconds(600.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromExpires) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay - base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromMaxAge) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(http_names::kCacheControl,
                                       AtomicString("max-age=600"));

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(base::Seconds(500.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromLastModified) {
  ResourceResponse expired200_response;
  expired200_response.SetHttpStatusCode(200);
  expired200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  expired200_response.SetHttpHeaderField(
      http_names::kLastModified, AtomicString(kOneDayBeforeOriginalRequest));

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock beyond the implicit freshness period.
  AdvanceClock(kOneDay * 0.2);

  EXPECT_FALSE(expired200->ErrorOccurred());
  MockResource* fetched = FetchMockResource();
  // We want to make sure that revalidation happens, and we are checking the
  // ResourceStatus because in this case the revalidation request fails
  // synchronously.
  EXPECT_EQ(expired200, fetched);
  EXPECT_TRUE(expired200->ErrorOccurred());
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHttpStatusCode(200);
  expired200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  expired200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay + base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource hasn't been loaded in this "document" before, then it
// shouldn't have list of available resources logic.
TEST_F(MemoryCacheCorrectnessTest, NewMockResourceExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHttpStatusCode(200);
  expired200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  expired200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay + base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource has been loaded in this "document" before, then it should
// have list of available resources logic, and so normal cache testing should be
// bypassed.
TEST_F(MemoryCacheCorrectnessTest, ReuseMockResourceExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHttpStatusCode(200);
  expired200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  expired200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the freshness period, and make a request to add
  // this resource to the document resources.
  AdvanceClock(base::Seconds(15.));
  MockResource* first_fetched = FetchMockResource();
  EXPECT_EQ(expired200, first_fetched);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay + base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromMaxAge) {
  ResourceResponse expired200_response;
  expired200_response.SetHttpStatusCode(200);
  expired200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  expired200_response.SetHttpHeaderField(http_names::kCacheControl,
                                         AtomicString("max-age=600"));

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(base::Seconds(700.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoCache) {
  ResourceResponse fresh200_nocache_response;
  fresh200_nocache_response.SetHttpStatusCode(200);
  fresh200_nocache_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_nocache_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));
  fresh200_nocache_response.SetHttpHeaderField(http_names::kCacheControl,
                                               AtomicString("no-cache"));

  MockResource* fresh200_nocache =
      ResourceFromResourceResponse(fresh200_nocache_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay - base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_nocache, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoCache) {
  ResourceRequest no_cache_request;
  no_cache_request.SetHttpHeaderField(http_names::kCacheControl,
                                      AtomicString("no-cache"));
  no_cache_request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* no_cache_resource =
      ResourceFromResourceRequest(std::move(no_cache_request));
  MockResource* fetched = FetchMockResource();
  EXPECT_NE(no_cache_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoStore) {
  ResourceResponse fresh200_nostore_response;
  fresh200_nostore_response.SetHttpStatusCode(200);
  fresh200_nostore_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_nostore_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));
  fresh200_nostore_response.SetHttpHeaderField(http_names::kCacheControl,
                                               AtomicString("no-store"));

  MockResource* fresh200_nostore =
      ResourceFromResourceResponse(fresh200_nostore_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay - base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_nostore, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoStore) {
  ResourceRequest no_store_request;
  no_store_request.SetHttpHeaderField(http_names::kCacheControl,
                                      AtomicString("no-store"));
  no_store_request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* no_store_resource =
      ResourceFromResourceRequest(std::move(no_store_request));
  MockResource* fetched = FetchMockResource();
  EXPECT_NE(no_store_resource, fetched);
}

// FIXME: Determine if ignoring must-revalidate for blink is correct behaviour.
// See crbug.com/340088 .
TEST_F(MemoryCacheCorrectnessTest, DISABLED_FreshButMustRevalidate) {
  ResourceResponse fresh200_must_revalidate_response;
  fresh200_must_revalidate_response.SetHttpStatusCode(200);
  fresh200_must_revalidate_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_must_revalidate_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));
  fresh200_must_revalidate_response.SetHttpHeaderField(
      http_names::kCacheControl, AtomicString("must-revalidate"));

  MockResource* fresh200_must_revalidate =
      ResourceFromResourceResponse(fresh200_must_revalidate_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(kOneDay - base::Seconds(15.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_must_revalidate, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithFreshRedirect) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  auto* first_resource = MakeGarbageCollected<MockResource>(request);

  ResourceResponse fresh301_response(redirect_url);
  fresh301_response.SetHttpStatusCode(301);
  fresh301_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh301_response.SetHttpHeaderField(http_names::kLocation,
                                       AtomicString(kRedirectTargetUrlString));
  fresh301_response.SetHttpHeaderField(http_names::kCacheControl,
                                       AtomicString("max-age=600"));

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh301_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(base::Seconds(500.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithStaleRedirect) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  auto* first_resource = MakeGarbageCollected<MockResource>(request);

  ResourceResponse stale301_response(redirect_url);
  stale301_response.SetHttpStatusCode(301);
  stale301_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  stale301_response.SetHttpHeaderField(http_names::kLocation,
                                       AtomicString(kRedirectTargetUrlString));

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, stale301_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(base::Seconds(500.));

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, PostToSameURLTwice) {
  ResourceRequest request1{KURL(kResourceURL)};
  request1.SetHttpMethod(http_names::kPOST);
  request1.SetRequestorOrigin(GetSecurityOrigin());
  RawResource* resource1 =
      RawResource::CreateForTest(request1, ResourceType::kRaw);
  resource1->SetStatus(ResourceStatus::kPending);
  AddResourceToMemoryCache(resource1);

  ResourceRequest request2{KURL(kResourceURL)};
  request2.SetHttpMethod(http_names::kPOST);
  request2.SetRequestorOrigin(GetSecurityOrigin());
  FetchParameters fetch2 = FetchParameters::CreateForTest(std::move(request2));
  RawResource* resource2 = RawResource::FetchSynchronously(fetch2, Fetcher());
  EXPECT_NE(resource1, resource2);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectNotImplicitlyFresh) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  RawResource* first_resource = RawResource::CreateForTest(
      redirect_url, GetSecurityOrigin(), ResourceType::kRaw);

  ResourceResponse fresh302_response(redirect_url);
  fresh302_response.SetHttpStatusCode(302);
  fresh302_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh302_response.SetHttpHeaderField(
      http_names::kLastModified, AtomicString(kOneDayBeforeOriginalRequest));
  fresh302_response.SetHttpHeaderField(http_names::kLocation,
                                       AtomicString(kRedirectTargetUrlString));

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(base::Seconds(500.));

  RawResource* fetched = FetchRawResource();
  EXPECT_NE(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshMaxAge) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  auto* first_resource = MakeGarbageCollected<MockResource>(request);

  ResourceResponse fresh302_response(redirect_url);
  fresh302_response.SetHttpStatusCode(302);
  fresh302_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh302_response.SetHttpHeaderField(http_names::kCacheControl,
                                       AtomicString("max-age=600"));
  fresh302_response.SetHttpHeaderField(http_names::kLocation,
                                       AtomicString(kRedirectTargetUrlString));

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(base::Seconds(500.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshExpires) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  auto* first_resource = MakeGarbageCollected<MockResource>(request);

  ResourceResponse fresh302_response(redirect_url);
  fresh302_response.SetHttpStatusCode(302);
  fresh302_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh302_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));
  fresh302_response.SetHttpHeaderField(http_names::kLocation,
                                       AtomicString(kRedirectTargetUrlString));

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHttpStatusCode(200);
  fresh200_response.SetHttpHeaderField(
      http_names::kDate, AtomicString(kOriginalRequestDateAsString));
  fresh200_response.SetHttpHeaderField(
      http_names::kExpires, AtomicString(kOneDayAfterOriginalRequest));

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(base::Seconds(500.));

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

}  // namespace blink
