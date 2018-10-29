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
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

// An URL for the original request.
constexpr char kResourceURL[] = "http://resource.com/";

// The origin time of our first request.
constexpr char kOriginalRequestDateAsString[] = "Thu, 25 May 1977 18:30:00 GMT";
constexpr char kOneDayBeforeOriginalRequest[] = "Wed, 24 May 1977 18:30:00 GMT";
constexpr char kOneDayAfterOriginalRequest[] = "Fri, 26 May 1977 18:30:00 GMT";

}  // namespace

class MemoryCacheCorrectnessTest : public testing::Test {
 protected:
  MockResource* ResourceFromResourceResponse(ResourceResponse response) {
    if (response.Url().IsNull())
      response.SetURL(KURL(kResourceURL));
    ResourceRequest request(response.Url());
    request.SetRequestorOrigin(GetSecurityOrigin());
    MockResource* resource = MockResource::Create(request);
    resource->SetResponse(response);
    resource->FinishForTest();
    AddResourceToMemoryCache(resource);

    return resource;
  }
  MockResource* ResourceFromResourceRequest(ResourceRequest request) {
    if (request.Url().IsNull())
      request.SetURL(KURL(kResourceURL));
    request.SetFetchCredentialsMode(
        network::mojom::FetchCredentialsMode::kOmit);
    MockResource* resource = MockResource::Create(request);
    ResourceResponse response(KURL{kResourceURL});
    response.SetMimeType("text/html");
    resource->SetResponse(response);
    resource->FinishForTest();
    AddResourceToMemoryCache(resource);

    return resource;
  }
  void AddResourceToMemoryCache(Resource* resource) {
    GetMemoryCache()->Add(resource);
  }
  // TODO(toyoshim): Consider to use MockResource for all tests instead of
  // RawResource.
  RawResource* FetchRawResource() {
    ResourceRequest resource_request{KURL(kResourceURL)};
    resource_request.SetRequestContext(mojom::RequestContextType::INTERNAL);
    resource_request.SetRequestorOrigin(GetSecurityOrigin());
    FetchParameters fetch_params(resource_request);
    return RawResource::Fetch(fetch_params, Fetcher(), nullptr);
  }
  MockResource* FetchMockResource() {
    ResourceRequest resource_request{KURL(kResourceURL)};
    resource_request.SetRequestorOrigin(GetSecurityOrigin());
    FetchParameters fetch_params(resource_request);
    return MockResource::Fetch(fetch_params, Fetcher(), nullptr);
  }
  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }
  void AdvanceClock(double seconds) { platform_->AdvanceClockSeconds(seconds); }
  scoped_refptr<const SecurityOrigin> GetSecurityOrigin() const {
    return security_origin_;
  }

 private:
  // Overrides testing::Test.
  void SetUp() override {
    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ = ReplaceMemoryCacheForTesting(
        MemoryCache::Create(platform_->test_task_runner()));

    MockFetchContext* context =
        MockFetchContext::Create(MockFetchContext::kShouldNotLoadNewResource);
    security_origin_ = SecurityOrigin::CreateUniqueOpaque();
    context->SetSecurityOrigin(security_origin_);

    fetcher_ = ResourceFetcher::Create(context);
  }
  void TearDown() override {
    GetMemoryCache()->EvictResources();

    // Yield the ownership of the global memory cache back.
    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  }

  Persistent<MemoryCache> global_memory_cache_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  Persistent<ResourceFetcher> fetcher_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(MemoryCacheCorrectnessTest, FreshFromLastModified) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField("Last-Modified",
                                       kOneDayBeforeOriginalRequest);

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the implicit freshness period of this resource
  // before we make a request.
  AdvanceClock(600.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromExpires) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshFromMaxAge) {
  ResourceResponse fresh200_response;
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField("Cache-Control", "max-age=600");

  MockResource* fresh200 = ResourceFromResourceResponse(fresh200_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(500.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(fresh200, fetched);
}

// The strong validator causes a revalidation to be launched, and the proxy and
// original resources leak because of their reference loop.
TEST_F(MemoryCacheCorrectnessTest, DISABLED_ExpiredFromLastModified) {
  ResourceResponse expired200_response;
  expired200_response.SetHTTPStatusCode(200);
  expired200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200_response.SetHTTPHeaderField("Last-Modified",
                                         kOneDayBeforeOriginalRequest);

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock beyond the implicit freshness period.
  AdvanceClock(24. * 60. * 60. * 0.2);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHTTPStatusCode(200);
  expired200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200_response.SetHTTPHeaderField("Expires",
                                         kOneDayAfterOriginalRequest);

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource hasn't been loaded in this "document" before, then it
// shouldn't have list of available resources logic.
TEST_F(MemoryCacheCorrectnessTest, NewMockResourceExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHTTPStatusCode(200);
  expired200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200_response.SetHTTPHeaderField("Expires",
                                         kOneDayAfterOriginalRequest);

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

// If the resource has been loaded in this "document" before, then it should
// have list of available resources logic, and so normal cache testing should be
// bypassed.
TEST_F(MemoryCacheCorrectnessTest, ReuseMockResourceExpiredFromExpires) {
  ResourceResponse expired200_response;
  expired200_response.SetHTTPStatusCode(200);
  expired200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200_response.SetHTTPHeaderField("Expires",
                                         kOneDayAfterOriginalRequest);

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the freshness period, and make a request to add
  // this resource to the document resources.
  AdvanceClock(15.);
  MockResource* first_fetched = FetchMockResource();
  EXPECT_EQ(expired200, first_fetched);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. + 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, ExpiredFromMaxAge) {
  ResourceResponse expired200_response;
  expired200_response.SetHTTPStatusCode(200);
  expired200_response.SetHTTPHeaderField("Date", kOriginalRequestDateAsString);
  expired200_response.SetHTTPHeaderField("Cache-Control", "max-age=600");

  MockResource* expired200 = ResourceFromResourceResponse(expired200_response);

  // Advance the clock within the expiredness period of this resource before we
  // make a request.
  AdvanceClock(700.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(expired200, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoCache) {
  ResourceResponse fresh200_nocache_response;
  fresh200_nocache_response.SetHTTPStatusCode(200);
  fresh200_nocache_response.SetHTTPHeaderField(HTTPNames::Date,
                                               kOriginalRequestDateAsString);
  fresh200_nocache_response.SetHTTPHeaderField(HTTPNames::Expires,
                                               kOneDayAfterOriginalRequest);
  fresh200_nocache_response.SetHTTPHeaderField(HTTPNames::Cache_Control,
                                               "no-cache");

  MockResource* fresh200_nocache =
      ResourceFromResourceResponse(fresh200_nocache_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_nocache, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoCache) {
  ResourceRequest no_cache_request;
  no_cache_request.SetHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  no_cache_request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* no_cache_resource =
      ResourceFromResourceRequest(no_cache_request);
  MockResource* fetched = FetchMockResource();
  EXPECT_NE(no_cache_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshButNoStore) {
  ResourceResponse fresh200_nostore_response;
  fresh200_nostore_response.SetHTTPStatusCode(200);
  fresh200_nostore_response.SetHTTPHeaderField(HTTPNames::Date,
                                               kOriginalRequestDateAsString);
  fresh200_nostore_response.SetHTTPHeaderField(HTTPNames::Expires,
                                               kOneDayAfterOriginalRequest);
  fresh200_nostore_response.SetHTTPHeaderField(HTTPNames::Cache_Control,
                                               "no-store");

  MockResource* fresh200_nostore =
      ResourceFromResourceResponse(fresh200_nostore_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_nostore, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, RequestWithNoStore) {
  ResourceRequest no_store_request;
  no_store_request.SetHTTPHeaderField(HTTPNames::Cache_Control, "no-store");
  no_store_request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* no_store_resource =
      ResourceFromResourceRequest(no_store_request);
  MockResource* fetched = FetchMockResource();
  EXPECT_NE(no_store_resource, fetched);
}

// FIXME: Determine if ignoring must-revalidate for blink is correct behaviour.
// See crbug.com/340088 .
TEST_F(MemoryCacheCorrectnessTest, DISABLED_FreshButMustRevalidate) {
  ResourceResponse fresh200_must_revalidate_response;
  fresh200_must_revalidate_response.SetHTTPStatusCode(200);
  fresh200_must_revalidate_response.SetHTTPHeaderField(
      HTTPNames::Date, kOriginalRequestDateAsString);
  fresh200_must_revalidate_response.SetHTTPHeaderField(
      HTTPNames::Expires, kOneDayAfterOriginalRequest);
  fresh200_must_revalidate_response.SetHTTPHeaderField(HTTPNames::Cache_Control,
                                                       "must-revalidate");

  MockResource* fresh200_must_revalidate =
      ResourceFromResourceResponse(fresh200_must_revalidate_response);

  // Advance the clock within the freshness period of this resource before we
  // make a request.
  AdvanceClock(24. * 60. * 60. - 15.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(fresh200_must_revalidate, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithFreshRedirect) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* first_resource = MockResource::Create(request);

  ResourceResponse fresh301_response(redirect_url);
  fresh301_response.SetHTTPStatusCode(301);
  fresh301_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh301_response.SetHTTPHeaderField(HTTPNames::Location,
                                       kRedirectTargetUrlString);
  fresh301_response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh301_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(500.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, FreshWithStaleRedirect) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  request.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kOmit);
  MockResource* first_resource = MockResource::Create(request);

  ResourceResponse stale301_response(redirect_url);
  stale301_response.SetHTTPStatusCode(301);
  stale301_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  stale301_response.SetHTTPHeaderField(HTTPNames::Location,
                                       kRedirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, stale301_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(500.);

  MockResource* fetched = FetchMockResource();
  EXPECT_NE(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, PostToSameURLTwice) {
  ResourceRequest request1{KURL(kResourceURL)};
  request1.SetHTTPMethod(HTTPNames::POST);
  request1.SetRequestorOrigin(GetSecurityOrigin());
  RawResource* resource1 =
      RawResource::CreateForTest(request1, ResourceType::kRaw);
  resource1->SetStatus(ResourceStatus::kPending);
  AddResourceToMemoryCache(resource1);

  ResourceRequest request2{KURL(kResourceURL)};
  request2.SetHTTPMethod(HTTPNames::POST);
  request2.SetRequestorOrigin(GetSecurityOrigin());
  FetchParameters fetch2(request2);
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
  fresh302_response.SetHTTPStatusCode(302);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Last_Modified,
                                       kOneDayBeforeOriginalRequest);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Location,
                                       kRedirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(500.);

  RawResource* fetched = FetchRawResource();
  EXPECT_NE(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshMaxAge) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* first_resource = MockResource::Create(request);

  ResourceResponse fresh302_response(redirect_url);
  fresh302_response.SetHTTPStatusCode(302);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");
  fresh302_response.SetHTTPHeaderField(HTTPNames::Location,
                                       kRedirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  redirect_request.SetRequestorOrigin(GetSecurityOrigin());
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(500.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

TEST_F(MemoryCacheCorrectnessTest, 302RedirectExplicitlyFreshExpires) {
  KURL redirect_url(kResourceURL);
  const char kRedirectTargetUrlString[] = "http://redirect-target.com";
  KURL redirect_target_url(kRedirectTargetUrlString);

  ResourceRequest request(redirect_url);
  request.SetRequestorOrigin(GetSecurityOrigin());
  MockResource* first_resource = MockResource::Create(request);

  ResourceResponse fresh302_response(redirect_url);
  fresh302_response.SetHTTPStatusCode(302);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);
  fresh302_response.SetHTTPHeaderField(HTTPNames::Location,
                                       kRedirectTargetUrlString);

  // Add the redirect to our request.
  ResourceRequest redirect_request = ResourceRequest(redirect_target_url);
  first_resource->WillFollowRedirect(redirect_request, fresh302_response);

  // Add the final response to our request.
  ResourceResponse fresh200_response(redirect_target_url);
  fresh200_response.SetHTTPStatusCode(200);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Date,
                                       kOriginalRequestDateAsString);
  fresh200_response.SetHTTPHeaderField(HTTPNames::Expires,
                                       kOneDayAfterOriginalRequest);

  first_resource->SetResponse(fresh200_response);
  first_resource->FinishForTest();
  AddResourceToMemoryCache(first_resource);

  AdvanceClock(500.);

  MockResource* fetched = FetchMockResource();
  EXPECT_EQ(first_resource, fetched);
}

}  // namespace blink
