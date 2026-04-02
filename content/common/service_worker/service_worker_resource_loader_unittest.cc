// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

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

}  // namespace content
