// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <stdint.h>

#include <limits>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/http/http_request_info.h"
#include "net/third_party/quiche/src/quiche/spdy/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/spdy/test_tools/spdy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

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

INSTANTIATE_TEST_SUITE_P(All,
                         SpdyHttpUtilsTestParam,
                         testing::Values(true, false));

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
  spdy::Http2HeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::HIGHEST,
                                   request.extra_headers, &headers);
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

TEST_P(SpdyHttpUtilsTestParam, CreateSpdyHeadersWithExistingPriority) {
  GURL url("https://www.google.com/index.html");
  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.priority_incremental = true;
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent, "Chrome/1.1");
  request.extra_headers.SetHeader(net::kHttp2PriorityHeader,
                                  "explicit-priority");
  spdy::Http2HeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::HIGHEST,
                                   request.extra_headers, &headers);
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
  spdy::Http2HeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, RequestPriority::DEFAULT_PRIORITY,
                                   request.extra_headers, &headers);
  EXPECT_EQ("CONNECT", headers[":method"]);
  EXPECT_TRUE(headers.end() == headers.find(":scheme"));
  EXPECT_EQ("www.google.com:443", headers[":authority"]);
  EXPECT_EQ(headers.end(), headers.find(":path"));
  EXPECT_EQ(headers.end(), headers.find(":scheme"));
  EXPECT_TRUE(headers.end() == headers.find(":version"));
  EXPECT_EQ("Chrome/1.1", headers["user-agent"]);
}

}  // namespace net
