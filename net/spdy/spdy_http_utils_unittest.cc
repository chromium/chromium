// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_http_utils.h"

#include <stdint.h>

#include <limits>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_headers_test_util.h"
#include "net/http/http_response_info.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using ::testing::Values;

class SpdyHttpUtilsTestParam : public testing::TestWithParam<bool> {
 public:
  SpdyHttpUtilsTestParam() {
    if (PriorityHeaderEnabled()) {
      feature_list_.InitAndEnableFeature(net::features::kPriorityHeader);
    } else {
      feature_list_.InitAndDisableFeature(net::features::kPriorityHeader);
    }
  }

 protected:
  bool PriorityHeaderEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, SpdyHttpUtilsTestParam, Values(true, false));

// Check that the headers are ordered correctly, with pseudo-headers
// preceding HTTP headers per
// https://datatracker.ietf.org/doc/html/rfc9114#section-4.3
void CheckOrdering(const quiche::HttpHeaderBlock& headers) {
  bool seen_http_header = false;

  for (auto& header : headers) {
    const bool is_pseudo = header.first.starts_with(':');
    if (is_pseudo) {
      ASSERT_FALSE(seen_http_header) << "Header order is incorrect:\n"
                                     << headers.DebugString();
    } else {
      seen_http_header = true;
    }
  }
}

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy3Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(LOWEST));
  EXPECT_EQ(4, ConvertRequestPriorityToSpdyPriority(IDLE));
  EXPECT_EQ(5, ConvertRequestPriorityToSpdyPriority(THROTTLED));
}

TEST(SpdyHttpUtilsTest, ConvertSpdy3PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2));
  EXPECT_EQ(LOWEST, ConvertSpdyPriorityToRequestPriority(3));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(4));
  EXPECT_EQ(THROTTLED, ConvertSpdyPriorityToRequestPriority(5));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 6; i < std::numeric_limits<uint8_t>::max(); ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i));
  }
}

TEST_P(SpdyHttpUtilsTestParam, CreateSpdyHeadersFromHttpRequestHTTP2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.priority_incremental = true;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::HIGHEST,
                                   request.extra_headers, &headers);
  CheckOrdering(headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  if (base::FeatureList::IsEnabled(net::features::kPriorityHeader)) {
    EXPECT_EQ("u=0, i", headers[net::kHttp2PriorityHeader]);
  } else {
    EXPECT_EQ(headers.end(), headers.find(net::kHttp2PriorityHeader));
  }
  EXPECT_EQ(headers.end(), headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST_P(SpdyHttpUtilsTestParam,
       CreateSpdyHeadersFromHttpRequestForExtendedConnect) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "CONNECT";
  request.url = url;
  request.priority_incremental = true;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequestForExtendedConnect(
      request, RequestPriority::HIGHEST, "connect-ftp", request.extra_headers,
      &headers);
  CheckOrdering(headers);
  EXPECT_EQ("CONNECT", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("connect-ftp", headers[":protocol"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  if (base::FeatureList::IsEnabled(net::features::kPriorityHeader)) {
    EXPECT_EQ("u=0, i", headers[net::kHttp2PriorityHeader]);
  } else {
    EXPECT_EQ(headers.end(), headers.find(net::kHttp2PriorityHeader));
  }
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST_P(SpdyHttpUtilsTestParam, CreateSpdyHeadersWithDefaultPriority) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.priority_incremental = false;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::DEFAULT_PRIORITY,
                                   request.extra_headers, &headers);
  CheckOrdering(headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_FALSE(headers.contains(net::kHttp2PriorityHeader));
  EXPECT_FALSE(headers.contains(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST_P(SpdyHttpUtilsTestParam, CreateSpdyHeadersWithExistingPriority) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.priority_incremental = true;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  request.extra_headers.SetHeader(net::kHttp2PriorityHeader,
                                  "explicit-priority");
  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::HIGHEST,
                                   request.extra_headers, &headers);
  CheckOrdering(headers);
  EXPECT_EQ("GET", headers[":method"]);
  EXPECT_EQ("https", headers[":scheme"]);
  EXPECT_EQ("www.google.com", headers[":authority"]);
  EXPECT_EQ("/index.html", headers[":path"]);
  EXPECT_EQ("explicit-priority", headers[net::kHttp2PriorityHeader]);
  EXPECT_EQ(headers.end(), headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

TEST(SpdyHttpUtilsTest, CreateSpdyHeadersFromHttpRequestConnectHTTP2) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "CONNECT";
  request.url = url;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::DEFAULT_PRIORITY,
                                   request.extra_headers, &headers);
  CheckOrdering(headers);
  EXPECT_EQ("CONNECT", headers[":method"]);
  EXPECT_TRUE(headers.end() == headers.find(":scheme"));
  EXPECT_EQ("www.google.com:443", headers[":authority"]);
  EXPECT_EQ(headers.end(), headers.find(":path"));
  EXPECT_EQ(headers.end(), headers.find(":scheme"));
  EXPECT_TRUE(headers.end() == headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

constexpr auto ToSimpleString = test::HttpResponseHeadersToSimpleString;

enum class SpdyHeadersToHttpResponseHeadersFeatureConfig {
  kUseRawString,
  kUseBuilder
};

std::string PrintToString(
    SpdyHeadersToHttpResponseHeadersFeatureConfig config) {
  switch (config) {
    case SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseRawString:
      return "RawString";

    case SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseBuilder:
      return "UseBuilder";
  }
}

class SpdyHeadersToHttpResponseTest
    : public ::testing::TestWithParam<
          SpdyHeadersToHttpResponseHeadersFeatureConfig> {
 public:
  SpdyHeadersToHttpResponseTest() {
    switch (GetParam()) {
      case SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseRawString:
        feature_list_.InitWithFeatures(
            {}, {features::kSpdyHeadersToHttpResponseUseBuilder});
        break;

      case SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseBuilder:
        feature_list_.InitWithFeatures(
            {features::kSpdyHeadersToHttpResponseUseBuilder}, {});
        break;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test behaves the same regardless of which features are enabled.
TEST_P(SpdyHeadersToHttpResponseTest, SpdyHeadersToHttpResponse) {
  constexpr char kExpectedSimpleString[] =
      "HTTP/1.1 200\n"
      "content-type: text/html\n"
      "cache-control: no-cache, no-store\n"
      "set-cookie: test_cookie=1234567890; Max-Age=3600; Secure; HttpOnly\n"
      "set-cookie: session_id=abcdefghijklmnopqrstuvwxyz; Path=/; HttpOnly\n";
  quiche::HttpHeaderBlock input;
  input[spdy::kHttp2StatusHeader] = "200";
  input["content-type"] = "text/html";
  input["cache-control"] = "no-cache, no-store";
  input.AppendValueOrAddHeader(
      "set-cookie", "test_cookie=1234567890; Max-Age=3600; Secure; HttpOnly");
  input.AppendValueOrAddHeader(
      "set-cookie", "session_id=abcdefghijklmnopqrstuvwxyz; Path=/; HttpOnly");

  net::HttpResponseInfo output;
  output.remote_endpoint = {{127, 0, 0, 1}, 80};

  EXPECT_EQ(OK, SpdyHeadersToHttpResponse(input, &output));

  // This should be set.
  EXPECT_TRUE(output.was_fetched_via_spdy);

  // This should be untouched.
  EXPECT_EQ(output.remote_endpoint, IPEndPoint({127, 0, 0, 1}, 80));

  EXPECT_EQ(kExpectedSimpleString, ToSimpleString(output.headers));
}

INSTANTIATE_TEST_SUITE_P(
    SpdyHttpUtils,
    SpdyHeadersToHttpResponseTest,
    Values(SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseRawString,
           SpdyHeadersToHttpResponseHeadersFeatureConfig::kUseBuilder),
    ::testing::PrintToStringParamName());

// TODO(ricea): Once SpdyHeadersToHttpResponseHeadersUsingRawString has been
// removed, remove the parameterization and make these into
// SpdyHeadersToHttpResponse tests.

using SpdyHeadersToHttpResponseHeadersFunctionPtrType =
    base::expected<scoped_refptr<HttpResponseHeaders>, int> (*)(
        const quiche::HttpHeaderBlock&);

class SpdyHeadersToHttpResponseHeadersTest
    : public testing::TestWithParam<
          SpdyHeadersToHttpResponseHeadersFunctionPtrType> {
 public:
  base::expected<scoped_refptr<HttpResponseHeaders>, int> PerformConversion(
      const quiche::HttpHeaderBlock& headers) {
    return GetParam()(headers);
  }
};

TEST_P(SpdyHeadersToHttpResponseHeadersTest, NoStatus) {
  quiche::HttpHeaderBlock headers;
  EXPECT_THAT(PerformConversion(headers),
              base::test::ErrorIs(ERR_INCOMPLETE_HTTP2_HEADERS));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, EmptyStatus) {
  constexpr char kRawHeaders[] = "HTTP/1.1 200\n";
  quiche::HttpHeaderBlock headers;
  headers[":status"] = "";
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, Plain200) {
  // ":status" does not appear as a header in the output.
  constexpr char kRawHeaders[] = "HTTP/1.1 200\n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, MultipleLocation) {
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "304";
  headers["Location"] = "https://example.com/1";
  headers.AppendValueOrAddHeader("location", "https://example.com/2");
  EXPECT_THAT(PerformConversion(headers),
              base::test::ErrorIs(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, SpacesAmongValues) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "spaces: foo  ,   bar\n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers["spaces"] = "foo  ,   bar";
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, RepeatedHeader) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "name: value1\n"
      "name: value2\n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers.AppendValueOrAddHeader("name", "value1");
  headers.AppendValueOrAddHeader("name", "value2");
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, EmptyValue) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "empty: \n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers.AppendValueOrAddHeader("empty", "");
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, PseudoHeadersAreDropped) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "Content-Length: 5\n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers[spdy::kHttp2MethodHeader] = "GET";
  headers["Content-Length"] = "5";
  headers[":fake"] = "ignored";
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest, DoubleEmptyLocationHeader) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "location: \n"
      "location: \n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers.AppendValueOrAddHeader("location", "");
  headers.AppendValueOrAddHeader("location", "");
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

TEST_P(SpdyHeadersToHttpResponseHeadersTest,
       DifferentLocationHeaderTriggersError) {
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers.AppendValueOrAddHeader("location", "https://same/");
  headers.AppendValueOrAddHeader("location", "https://same/");
  headers.AppendValueOrAddHeader("location", "https://different/");
  EXPECT_THAT(PerformConversion(headers),
              base::test::ErrorIs(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION));
}

// TODO(ricea): Ensure that QUICHE will never send us header values with leading
// or trailing whitespace and remove this test.
TEST_P(SpdyHeadersToHttpResponseHeadersTest,
       LocationEquivalenceIgnoresSurroundingSpace) {
  constexpr char kRawHeaders[] =
      "HTTP/1.1 200\n"
      "location: https://same/\n"
      "location: https://same/\n";
  quiche::HttpHeaderBlock headers;
  headers[spdy::kHttp2StatusHeader] = "200";
  headers.AppendValueOrAddHeader("location", " https://same/");
  headers.AppendValueOrAddHeader("location", "https://same/ ");
  ASSERT_OK_AND_ASSIGN(const auto output, PerformConversion(headers));
  EXPECT_EQ(kRawHeaders, ToSimpleString(output));
}

INSTANTIATE_TEST_SUITE_P(
    SpdyHttpUtils,
    SpdyHeadersToHttpResponseHeadersTest,
    Values(SpdyHeadersToHttpResponseHeadersUsingRawString,
           SpdyHeadersToHttpResponseHeadersUsingBuilder),
    [](const testing::TestParamInfo<
        SpdyHeadersToHttpResponseHeadersTest::ParamType>& info) {
      return info.param == SpdyHeadersToHttpResponseHeadersUsingRawString
                 ? "SpdyHeadersToHttpResponseHeadersUsingRawString"
                 : "SpdyHeadersToHttpResponseHeadersUsingBuilder";
    });

}  // namespace

}  // namespace net
