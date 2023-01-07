// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FetchResponseDataTest : public testing::Test {
 public:
  FetchResponseData* CreateInternalResponse() {
    FetchResponseData* internal_response = FetchResponseData::Create();
    internal_response->SetStatus(200);
    Vector<KURL> url_list;
    url_list.push_back(KURL("http://www.example.com"));
    internal_response->SetURLList(url_list);
    internal_response->HeaderList()->Append("set-cookie", "foo");
    internal_response->HeaderList()->Append("bar", "bar");
    internal_response->HeaderList()->Append("cache-control", "no-cache");
    return internal_response;
  }

  void CheckHeaders(const mojom::blink::FetchAPIResponse& response) {
    EXPECT_EQ("foo", response.headers.at("set-cookie"));
    EXPECT_EQ("bar", response.headers.at("bar"));
    EXPECT_EQ("no-cache", response.headers.at("cache-control"));
  }
};

TEST_F(FetchResponseDataTest, HeaderList) {
  FetchResponseData* response_data = CreateInternalResponse();

  String set_cookie_value;
  ASSERT_TRUE(response_data->HeaderList()->Get("set-cookie", set_cookie_value));
  EXPECT_EQ("foo", set_cookie_value);

  String bar_value;
  ASSERT_TRUE(response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);

  String cache_control_value;
  ASSERT_TRUE(
      response_data->HeaderList()->Get("cache-control", cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest, ToFetchAPIResponseDefaultType) {
  FetchResponseData* internal_response = CreateInternalResponse();

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      internal_response->PopulateFetchAPIResponse(KURL());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            fetch_api_response->response_type);
  CheckHeaders(*fetch_api_response);
}

TEST_F(FetchResponseDataTest, BasicFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* basic_response_data =
      internal_response->CreateBasicFilteredResponse();

  EXPECT_EQ(internal_response, basic_response_data->InternalResponse());

  EXPECT_FALSE(basic_response_data->HeaderList()->Has("set-cookie"));

  String bar_value;
  ASSERT_TRUE(basic_response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);

  String cache_control_value;
  ASSERT_TRUE(basic_response_data->HeaderList()->Get("cache-control",
                                                     cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest, ToFetchAPIResponseBasicType) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* basic_response_data =
      internal_response->CreateBasicFilteredResponse();

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      basic_response_data->PopulateFetchAPIResponse(KURL());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            fetch_api_response->response_type);
  CheckHeaders(*fetch_api_response);
}

TEST_F(FetchResponseDataTest, CorsFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse(HTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest,
       CorsFilterOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse({"set-cookie", "bar"});

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  String bar_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);
}

TEST_F(FetchResponseDataTest, CorsFilterWithEmptyHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse(HTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest,
       CorsFilterWithEmptyHeaderSetOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse(HTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest, CorsFilterWithExplicitHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  HTTPHeaderSet exposed_headers;
  exposed_headers.insert("set-cookie");
  exposed_headers.insert("bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse(exposed_headers);

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  String bar_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);
}

TEST_F(FetchResponseDataTest, ToFetchAPIResponseCorsType) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCorsFilteredResponse(HTTPHeaderSet());

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      cors_response_data->PopulateFetchAPIResponse(KURL());
  EXPECT_EQ(network::mojom::FetchResponseType::kCors,
            fetch_api_response->response_type);
  CheckHeaders(*fetch_api_response);
}

TEST_F(FetchResponseDataTest, OpaqueFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("set-cookie"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("bar"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("cache-control"));
}

TEST_F(FetchResponseDataTest, OpaqueRedirectFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueRedirectFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_EQ(opaque_response_data->HeaderList()->size(), 0u);
  EXPECT_EQ(*opaque_response_data->Url(), *internal_response->Url());
}

TEST_F(FetchResponseDataTest,
       OpaqueFilterOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  EXPECT_EQ(internal_response, opaque_response_data->InternalResponse());

  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("set-cookie"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("bar"));
  EXPECT_FALSE(opaque_response_data->HeaderList()->Has("cache-control"));
}

TEST_F(FetchResponseDataTest, ToFetchAPIResponseOpaqueType) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      opaque_response_data->PopulateFetchAPIResponse(KURL());
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaque,
            fetch_api_response->response_type);
  CheckHeaders(*fetch_api_response);
}

TEST_F(FetchResponseDataTest, ToFetchAPIResponseOpaqueRedirectType) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_redirect_response_data =
      internal_response->CreateOpaqueRedirectFilteredResponse();

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      opaque_redirect_response_data->PopulateFetchAPIResponse(KURL());
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaqueRedirect,
            fetch_api_response->response_type);
  CheckHeaders(*fetch_api_response);
}

TEST_F(FetchResponseDataTest, DefaultResponseTime) {
  FetchResponseData* internal_response = CreateInternalResponse();
  EXPECT_FALSE(internal_response->ResponseTime().is_null());
}

TEST_F(FetchResponseDataTest, ContentSecurityPolicy) {
  base::test::ScopedFeatureList scoped_feature_list;
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("content-security-policy",
                                          "frame-ancestors 'none'");
  internal_response->HeaderList()->Append("content-security-policy-report-only",
                                          "frame-ancestors 'none'");

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      internal_response->PopulateFetchAPIResponse(
          KURL("https://www.example.org"));
  auto& csp = fetch_api_response->parsed_headers->content_security_policy;

  EXPECT_EQ(csp.size(), 2U);
  EXPECT_EQ(csp[0]->header->type,
            network::mojom::ContentSecurityPolicyType::kEnforce);
  EXPECT_EQ(csp[1]->header->type,
            network::mojom::ContentSecurityPolicyType::kReport);
}

TEST_F(FetchResponseDataTest, AuthChallengeInfo) {
  FetchResponseData* internal_response = CreateInternalResponse();
  net::AuthChallengeInfo auth_challenge_info;
  auth_challenge_info.is_proxy = true;
  auth_challenge_info.challenge = "foobar";
  internal_response->SetAuthChallengeInfo(auth_challenge_info);

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      internal_response->PopulateFetchAPIResponse(KURL());
  ASSERT_TRUE(fetch_api_response->auth_challenge_info.has_value());
  EXPECT_TRUE(fetch_api_response->auth_challenge_info->is_proxy);
  EXPECT_EQ("foobar", fetch_api_response->auth_challenge_info->challenge);
}

}  // namespace blink
