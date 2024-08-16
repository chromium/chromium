// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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

TEST(FetchRequestDataTest, CheckTrustTokenParamsAreCopiedWithCreate) {
  test::TaskEnvironment task_environment;
  // create a fetch API request instance
  auto request = mojom::blink::FetchAPIRequest::New();
  // create a TrustTokenParams instance
  WTF::Vector<::scoped_refptr<const ::blink::SecurityOrigin>> issuers;
  issuers.push_back(
      ::blink::SecurityOrigin::CreateFromString("https://aaa.example"));
  issuers.push_back(
      ::blink::SecurityOrigin::CreateFromString("https://bbb.example"));
  WTF::Vector<WTF::String> additional_signed_headers = {"aaa", "bbb"};
  auto trust_token_params = network::mojom::blink::TrustTokenParams::New(
      network::mojom::TrustTokenOperationType::kRedemption,
      network::mojom::TrustTokenRefreshPolicy::kUseCached,
      /* custom_key_commitment=*/"custom_key_commitment",
      /* custom_issuer=*/
      ::blink::SecurityOrigin::CreateFromString("https://ccc.example"),
      network::mojom::TrustTokenSignRequestData::kInclude,
      /* include_timestamp_header=*/true, issuers, additional_signed_headers,
      /* possibly_unsafe_additional_signing_data=*/"ccc");
  // get a copy of of TrustTokenParams instance created, will be used in testing
  // later
  auto trust_token_params_copy = trust_token_params->Clone();
  // set trust token params in request
  request->trust_token_params = std::move(trust_token_params);
  // create a FetchRequestData instance from request
  FetchRequestData* request_data = FetchRequestData::Create(
      /*script_state=*/nullptr, std::move(request),
      FetchRequestData::ForServiceWorkerFetchEvent::kTrue);
  // compare trust token params of request_data to trust_token_params_copy.
  EXPECT_TRUE(request_data->TrustTokenParams());
  EXPECT_EQ(*(request_data->TrustTokenParams()), *(trust_token_params_copy));
}

TEST(FetchRequestDataTest, CheckServiceworkerRaceNetworkRequestToken) {
  test::TaskEnvironment task_environment;
  // create a fetch API request instance
  auto request = PrepareFetchAPIRequest();
  const base::UnguessableToken token = base::UnguessableToken::Create();
  request->service_worker_race_network_request_token = token;

  // Create FetchRequestData
  FetchRequestData* request_data = FetchRequestData::Create(
      /*script_state=*/nullptr, std::move(request),
      FetchRequestData::ForServiceWorkerFetchEvent::kTrue);
  EXPECT_EQ(token, request_data->ServiceWorkerRaceNetworkRequestToken());

  // Token is not cloned.
  auto exception_state = ExceptionState(
      nullptr, ExceptionContext(v8::ExceptionContext::kUnknown, nullptr));
  auto* cloned_request_data = request_data->Clone(nullptr, exception_state);
  EXPECT_TRUE(
      cloned_request_data->ServiceWorkerRaceNetworkRequestToken().is_empty());
}

}  // namespace blink
