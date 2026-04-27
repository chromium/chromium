// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

namespace {

class TestServiceWorkerResourceLoader : public ServiceWorkerResourceLoader {
 public:
  explicit TestServiceWorkerResourceLoader(bool is_main_resource)
      : is_main_resource_(is_main_resource) {}
  ~TestServiceWorkerResourceLoader() override = default;

  bool IsMainResourceLoader() override { return is_main_resource_; }

  void CommitResponseBody(
      const network::mojom::URLResponseHeadPtr& response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {}

  void CommitEmptyResponseAndComplete() override {}

  void CommitCompleted(int error_code, const char* reason) override {}

  void HandleRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHeadPtr& response_head) override {}

 private:
  bool is_main_resource_;
};

}  // namespace

TEST(ServiceWorkerResourceLoaderTest, IsValidServiceWorkerResponse) {
  auto request_mode = network::mojom::RequestMode::kSameOrigin;
  auto redirect_mode = network::mojom::RedirectMode::kFollow;

  // Normal response.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kDefault;
    EXPECT_TRUE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, redirect_mode, response));
  }

  // Error response.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kError;
    EXPECT_FALSE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, redirect_mode, response));
  }

  // same-origin request and cors response.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kCors;
    EXPECT_FALSE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        network::mojom::RequestMode::kSameOrigin, redirect_mode, response));
  }

  // cross-origin request (cors) and cors response (OK).
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kCors;
    EXPECT_TRUE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        network::mojom::RequestMode::kCors, redirect_mode, response));
  }

  // cors request and opaque response.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kOpaque;
    EXPECT_FALSE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        network::mojom::RequestMode::kCors, redirect_mode, response));
  }

  // no-cors request and opaque response (OK).
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type = network::mojom::FetchResponseType::kOpaque;
    EXPECT_TRUE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        network::mojom::RequestMode::kNoCors, redirect_mode, response));
  }

  // opaqueredirect response and manual redirect mode (OK).
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type =
        network::mojom::FetchResponseType::kOpaqueRedirect;
    EXPECT_TRUE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, network::mojom::RedirectMode::kManual, response));
  }

  // opaqueredirect response and follow redirect mode.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->response_type =
        network::mojom::FetchResponseType::kOpaqueRedirect;
    EXPECT_FALSE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, network::mojom::RedirectMode::kFollow, response));
  }

  // multiple URLs in URL list and follow redirect mode (OK).
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->url_list.emplace_back("http://a.test/1");
    response->url_list.emplace_back("http://a.test/2");
    EXPECT_TRUE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, network::mojom::RedirectMode::kFollow, response));
  }

  // multiple URLs in URL list and manual redirect mode.
  {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->url_list.emplace_back("http://a.test/1");
    response->url_list.emplace_back("http://a.test/2");
    EXPECT_FALSE(ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
        request_mode, network::mojom::RedirectMode::kManual, response));
  }
}

TEST(ServiceWorkerResourceLoaderTest, IsValidStaticRouterResponse) {
  TestServiceWorkerResourceLoader loader(/*is_main_resource=*/false);

  network::ResourceRequest request;
  request.url = GURL("https://b.test/resource");
  request.request_initiator = url::Origin::Create(GURL("https://a.test/"));
  request.mode = network::mojom::RequestMode::kNoCors;
  request.destination = network::mojom::RequestDestination::kImage;

  auto response = blink::mojom::FetchAPIResponse::New();
  response->response_type = network::mojom::FetchResponseType::kOpaque;
  response->request_include_credentials = true;

  network::CrossOriginEmbedderPolicy coep;
  coep.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  network::DocumentIsolationPolicy dip;

  // Case 1: Same-origin request should be valid even without CORP.
  {
    base::HistogramTester histogram_tester;
    network::ResourceRequest same_origin_request;
    same_origin_request.url = GURL("https://a.test/resource");
    same_origin_request.request_initiator =
        url::Origin::Create(GURL("https://a.test/"));
    same_origin_request.mode = network::mojom::RequestMode::kNoCors;

    auto same_origin_response = blink::mojom::FetchAPIResponse::New();
    same_origin_response->response_type =
        network::mojom::FetchResponseType::kOpaque;
    same_origin_response->request_include_credentials = true;
    same_origin_response->url_list.emplace_back("https://a.test/resource");

    EXPECT_TRUE(loader.IsValidStaticRouterResponse(same_origin_request,
                                                   same_origin_response, coep,
                                                   nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kSuccess, 1);
  }

  // Case 2: Cross-origin request without CORP, flag OFF.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kServiceWorkerStaticRouterCORPCheck);

    auto cross_origin_response = blink::mojom::FetchAPIResponse::New();
    cross_origin_response->response_type =
        network::mojom::FetchResponseType::kOpaque;
    cross_origin_response->request_include_credentials = true;
    cross_origin_response->url_list.emplace_back("https://b.test/resource");

    EXPECT_TRUE(loader.IsValidStaticRouterResponse(
        request, cross_origin_response, coep, nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kViolation, 1);
  }

  // Case 3: Cross-origin request without CORP, flag ON.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kServiceWorkerStaticRouterCORPCheck);

    // Before the fix, this would have been TRUE because it used request.url
    // (a.test) which matches the initiator (a.test).
    // After the fix, it should be FALSE because it uses response->url_list
    // which contains b.test.
    network::ResourceRequest alias_request;
    alias_request.url = GURL("https://a.test/alias");
    alias_request.request_initiator =
        url::Origin::Create(GURL("https://a.test/"));
    alias_request.mode = network::mojom::RequestMode::kNoCors;

    auto cross_origin_response = blink::mojom::FetchAPIResponse::New();
    cross_origin_response->response_type =
        network::mojom::FetchResponseType::kOpaque;
    cross_origin_response->request_include_credentials = true;
    cross_origin_response->url_list.emplace_back("https://b.test/resource");

    EXPECT_FALSE(loader.IsValidStaticRouterResponse(
        alias_request, cross_origin_response, coep, nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kBlocked, 1);
  }

  // Case 4: Same-origin request via alias, should be valid.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kServiceWorkerStaticRouterCORPCheck);

    network::ResourceRequest alias_request;
    alias_request.url = GURL("https://a.test/alias");
    alias_request.request_initiator =
        url::Origin::Create(GURL("https://a.test/"));
    alias_request.mode = network::mojom::RequestMode::kNoCors;

    auto same_origin_response = blink::mojom::FetchAPIResponse::New();
    same_origin_response->response_type =
        network::mojom::FetchResponseType::kOpaque;
    same_origin_response->request_include_credentials = true;
    same_origin_response->url_list.emplace_back("https://a.test/resource");

    EXPECT_TRUE(loader.IsValidStaticRouterResponse(
        alias_request, same_origin_response, coep, nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kSuccess, 1);
  }

  // Case 5: Empty url_list (synthesized response), should be treated as
  // same-origin.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kServiceWorkerStaticRouterCORPCheck);

    network::ResourceRequest alias_request;
    alias_request.url = GURL("https://a.test/alias");
    alias_request.request_initiator =
        url::Origin::Create(GURL("https://a.test/"));
    alias_request.mode = network::mojom::RequestMode::kNoCors;

    auto synthesized_response = blink::mojom::FetchAPIResponse::New();
    synthesized_response->response_type =
        network::mojom::FetchResponseType::kDefault;
    synthesized_response->request_include_credentials = true;
    // url_list is empty.

    EXPECT_TRUE(loader.IsValidStaticRouterResponse(
        alias_request, synthesized_response, coep, nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kSuccess, 1);
  }

  // Case 6: Cross-origin request WITH CORP, flag ON.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kServiceWorkerStaticRouterCORPCheck);

    auto corp_response = blink::mojom::FetchAPIResponse::New();
    corp_response->response_type = network::mojom::FetchResponseType::kOpaque;
    corp_response->request_include_credentials = true;
    corp_response->url_list.emplace_back("https://b.test/resource");
    corp_response->headers["Cross-Origin-Resource-Policy"] = "cross-origin";

    EXPECT_TRUE(loader.IsValidStaticRouterResponse(request, corp_response, coep,
                                                   nullptr, dip, nullptr));
    histogram_tester.ExpectBucketCount(
        "ServiceWorker.StaticRouter.Subresource.CORPCheckResult",
        ServiceWorkerResourceLoader::CORPCheckResult::kSuccess, 1);
  }
}

}  // namespace content
