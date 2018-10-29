// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

#include "base/guid.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace content {

namespace {

TEST(ServiceWorkerRequestTest, SerialiazeDeserializeRoundTrip) {
  ServiceWorkerFetchRequest request(
      GURL("foo.com"), "GET", {{"User-Agent", "Chrome"}},
      Referrer(GURL("bar.com"),
               network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade),
      true);
  request.mode = network::mojom::FetchRequestMode::kSameOrigin;
  request.is_main_resource_load = true;
  request.request_context_type = blink::mojom::RequestContextType::IFRAME;
  request.credentials_mode = network::mojom::FetchCredentialsMode::kSameOrigin;
  request.cache_mode = blink::mojom::FetchCacheMode::kForceCache;
  request.redirect_mode = network::mojom::FetchRedirectMode::kManual;
  request.integrity = "integrity";
  request.keepalive = true;
  request.client_id = "42";

  EXPECT_EQ(
      ServiceWorkerUtils::SerializeFetchRequestToString(request),
      ServiceWorkerUtils::SerializeFetchRequestToString(
          ServiceWorkerUtils::DeserializeFetchRequestFromString(
              ServiceWorkerUtils::SerializeFetchRequestToString(request))));
}

}  // namespace

}  // namespace content
