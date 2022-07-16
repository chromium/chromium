// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"

namespace blink {

namespace {

mojom::blink::FetchAPIRequestPtr PrepareFetchAPIRequest() {
  auto request = mojom::blink::FetchAPIRequest::New();
  request->url = KURL("https://example.com");
  // "sec-fetch-" will be excluded forcibly for service worker fetch events.
  request->headers.insert(String("sec-fetch-xx"), String("xxx"));
  request->headers.insert(String("sec-fetch-yy"), String("xxx"));
  // "x-hi-hi" will be kept.
  request->headers.insert(String("x-hi-hi"), String("xxx"));
  return request;
}

}  // namespace

TEST(FetchRequestDataTest, For_ServiceWorkerFetchEvent_Headers) {
  FetchRequestData* request_data = FetchRequestData::Create(
      /*script_state=*/nullptr, PrepareFetchAPIRequest(),
      FetchRequestData::ForServiceWorkerFetchEvent::kTrue);
  EXPECT_EQ(1U, request_data->HeaderList()->size());
  EXPECT_TRUE(request_data->HeaderList()->Has("x-hi-hi"));
  EXPECT_FALSE(request_data->HeaderList()->Has("sec-fetch-xx"));
  EXPECT_FALSE(request_data->HeaderList()->Has("sec-fetch-yy"));
}

TEST(FetchRequestDataTest, Not_For_ServiceWorkerFetchEvent_Headers) {
  FetchRequestData* request_data = FetchRequestData::Create(
      /*script_state=*/nullptr, PrepareFetchAPIRequest(),
      FetchRequestData::ForServiceWorkerFetchEvent::kFalse);
  EXPECT_EQ(3U, request_data->HeaderList()->size());
  EXPECT_TRUE(request_data->HeaderList()->Has("x-hi-hi"));
  EXPECT_TRUE(request_data->HeaderList()->Has("sec-fetch-xx"));
  EXPECT_TRUE(request_data->HeaderList()->Has("sec-fetch-yy"));
}

}  // namespace blink
