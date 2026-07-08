// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace blink {

TEST(ServiceWorkerLoaderHelpersTest, GetHttpResponseHeaders) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->headers["Content-Type"] = "text/html";
  response->headers["X-Custom-Header"] = "CustomValue";

  scoped_refptr<net::HttpResponseHeaders> headers =
      ServiceWorkerLoaderHelpers::GetHttpResponseHeaders(*response);

  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ("OK", headers->GetStatusText());

  EXPECT_EQ("text/html", headers->GetNormalizedHeader("Content-Type"));
  EXPECT_EQ("CustomValue", headers->GetNormalizedHeader("X-Custom-Header"));
}

TEST(ServiceWorkerLoaderHelpersTest, GetHttpResponseHeaders_EmptyHeaders) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 404;
  response->status_text = "Not Found";

  scoped_refptr<net::HttpResponseHeaders> headers =
      ServiceWorkerLoaderHelpers::GetHttpResponseHeaders(*response);

  ASSERT_TRUE(headers);
  EXPECT_EQ(404, headers->response_code());
  EXPECT_EQ("Not Found", headers->GetStatusText());
}

TEST(ServiceWorkerLoaderHelpersTest, GetHttpResponseHeaders_CorsFiltered) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kCors;
  response->headers["Content-Type"] = "text/html";
  response->headers["Cache-Control"] = "no-store";
  response->headers["Server-Timing"] = "metric;desc=value";
  response->headers["Link"] = "</a>; rel=preload";
  response->headers["X-Exposed"] = "ok";
  response->headers["X-Hidden"] = "no";
  response->cors_exposed_header_names = {"x-exposed"};

  scoped_refptr<net::HttpResponseHeaders> headers =
      ServiceWorkerLoaderHelpers::GetHttpResponseHeaders(*response);

  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ("text/html", headers->GetNormalizedHeader("Content-Type"));
  EXPECT_EQ("no-store", headers->GetNormalizedHeader("Cache-Control"));
  EXPECT_EQ("ok", headers->GetNormalizedHeader("X-Exposed"));
  EXPECT_FALSE(headers->HasHeader("Server-Timing"));
  EXPECT_FALSE(headers->HasHeader("Link"));
  EXPECT_FALSE(headers->HasHeader("X-Hidden"));
}

TEST(ServiceWorkerLoaderHelpersTest, GetHttpResponseHeaders_CorsNoExposed) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kCors;
  response->headers["Content-Length"] = "10";
  response->headers["Content-Range"] = "bytes 0-9/10";
  response->headers["Server-Timing"] = "metric;desc=value";

  scoped_refptr<net::HttpResponseHeaders> headers =
      ServiceWorkerLoaderHelpers::GetHttpResponseHeaders(*response);

  ASSERT_TRUE(headers);
  EXPECT_EQ("10", headers->GetNormalizedHeader("Content-Length"));
  EXPECT_EQ("bytes 0-9/10", headers->GetNormalizedHeader("Content-Range"));
  EXPECT_FALSE(headers->HasHeader("Server-Timing"));
}

TEST(ServiceWorkerLoaderHelpersTest, SaveResponseInfo_CorsFiltered) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kCors;
  response->headers["Content-Type"] = "text/html";
  response->headers["Server-Timing"] = "metric;desc=value";
  response->cors_exposed_header_names = {"X-Exposed"};

  auto head = network::mojom::URLResponseHead::New();
  ServiceWorkerLoaderHelpers::SaveResponseInfo(*response, head.get());

  ASSERT_TRUE(head->headers);
  EXPECT_EQ("text/html", head->headers->GetNormalizedHeader("Content-Type"));
  EXPECT_FALSE(head->headers->HasHeader("Server-Timing"));
  EXPECT_EQ(network::mojom::FetchResponseType::kCors, head->response_type);
}

TEST(ServiceWorkerLoaderHelpersTest, SaveResponseInfo_BasicNotFiltered) {
  auto response = mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kBasic;
  response->headers["Content-Type"] = "text/html";
  response->headers["Server-Timing"] = "metric;desc=value";

  auto head = network::mojom::URLResponseHead::New();
  ServiceWorkerLoaderHelpers::SaveResponseInfo(*response, head.get());

  ASSERT_TRUE(head->headers);
  EXPECT_EQ("text/html", head->headers->GetNormalizedHeader("Content-Type"));
  EXPECT_EQ("metric;desc=value",
            head->headers->GetNormalizedHeader("Server-Timing"));
}

}  // namespace blink
