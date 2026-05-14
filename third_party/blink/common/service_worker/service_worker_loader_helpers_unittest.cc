// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"

#include "net/http/http_response_headers.h"
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

}  // namespace blink
