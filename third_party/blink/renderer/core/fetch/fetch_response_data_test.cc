// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
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

  void CheckHeaders(const WebServiceWorkerResponse& web_response) {
    EXPECT_STREQ("foo", web_response.GetHeader("set-cookie").Utf8().c_str());
    EXPECT_STREQ("bar", web_response.GetHeader("bar").Utf8().c_str());
    EXPECT_STREQ("no-cache",
                 web_response.GetHeader("cache-control").Utf8().c_str());
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

TEST_F(FetchResponseDataTest, ToWebServiceWorkerDefaultType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();

  internal_response->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            web_response.ResponseType());
  CheckHeaders(web_response);
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

TEST_F(FetchResponseDataTest, ToWebServiceWorkerBasicType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* basic_response_data =
      internal_response->CreateBasicFilteredResponse();

  basic_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, CORSFilter) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest,
       CORSFilterOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse({"set-cookie", "bar"});

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  String bar_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);
}

TEST_F(FetchResponseDataTest, CORSFilterWithEmptyHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest,
       CORSFilterWithEmptyHeaderSetOnResponseWithAccessControlExposeHeaders) {
  FetchResponseData* internal_response = CreateInternalResponse();
  internal_response->HeaderList()->Append("access-control-expose-headers",
                                          "set-cookie, bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("bar"));

  String cache_control_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("cache-control",
                                                    cache_control_value));
  EXPECT_EQ("no-cache", cache_control_value);
}

TEST_F(FetchResponseDataTest, CORSFilterWithExplicitHeaderSet) {
  FetchResponseData* internal_response = CreateInternalResponse();
  WebHTTPHeaderSet exposed_headers;
  exposed_headers.insert("set-cookie");
  exposed_headers.insert("bar");

  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(exposed_headers);

  EXPECT_EQ(internal_response, cors_response_data->InternalResponse());

  EXPECT_FALSE(cors_response_data->HeaderList()->Has("set-cookie"));

  String bar_value;
  ASSERT_TRUE(cors_response_data->HeaderList()->Get("bar", bar_value));
  EXPECT_EQ("bar", bar_value);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerCORSType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* cors_response_data =
      internal_response->CreateCORSFilteredResponse(WebHTTPHeaderSet());

  cors_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kCORS,
            web_response.ResponseType());
  CheckHeaders(web_response);
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

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_response_data =
      internal_response->CreateOpaqueFilteredResponse();

  opaque_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaque,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueRedirectType) {
  WebServiceWorkerResponse web_response;
  FetchResponseData* internal_response = CreateInternalResponse();
  FetchResponseData* opaque_redirect_response_data =
      internal_response->CreateOpaqueRedirectFilteredResponse();

  opaque_redirect_response_data->PopulateWebServiceWorkerResponse(web_response);
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaqueRedirect,
            web_response.ResponseType());
  CheckHeaders(web_response);
}

TEST_F(FetchResponseDataTest, DefaultResponseTime) {
  FetchResponseData* internal_response = CreateInternalResponse();
  EXPECT_FALSE(internal_response->ResponseTime().is_null());
}

}  // namespace blink
