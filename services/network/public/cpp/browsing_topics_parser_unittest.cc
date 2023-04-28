// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/browsing_topics_parser.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_NoObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  EXPECT_FALSE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_TrueValueObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Observe-Browsing-Topics: ?1\r\n");
  EXPECT_TRUE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_FalseValueObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Observe-Browsing-Topics: ?0\r\n");
  EXPECT_FALSE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_NotBooleanObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Observe-Browsing-Topics: 1\r\n");
  EXPECT_FALSE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_InvalidObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Observe-Browsing-Topics: !!!\r\n");
  EXPECT_FALSE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

TEST(BrowsingTopicsParserTest,
     ParseObserveBrowsingTopicsFromHeader_InvalidNormalizedHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Observe-Browsing-Topics: ?1\r\n"
          "Observe-Browsing-Topics: ?1\r\n");
  EXPECT_FALSE(ParseObserveBrowsingTopicsFromHeader(*headers));
}

}  // namespace network
