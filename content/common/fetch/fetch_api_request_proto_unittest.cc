// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fetch/fetch_api_request_proto.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(FetchAPIRequestProtoTest, SerialiazeDeserializeRoundTrip) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->mode = network::mojom::RequestMode::kSameOrigin;
  request->is_main_resource_load = true;
  request->url = GURL("foo.com");
  request->method = "GET";
  request->headers = {{"User-Agent", "Chrome"}};
  request->referrer = blink::mojom::Referrer::New(
      GURL("bar.com"),
      network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade);
  request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  request->cache_mode = blink::mojom::FetchCacheMode::kForceCache;
  request->redirect_mode = network::mojom::RedirectMode::kManual;
  request->integrity = "integrity";
  request->keepalive = true;
  request->is_reload = true;

  EXPECT_EQ(SerializeFetchRequestToString(*request),
            SerializeFetchRequestToString(*DeserializeFetchRequestFromString(
                SerializeFetchRequestToString(*request))));
}

}  // namespace content
