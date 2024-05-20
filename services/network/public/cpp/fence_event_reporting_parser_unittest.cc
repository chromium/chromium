// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/fence_event_reporting_parser.h"

#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(FenceEventReportingParserTest,
     ParseAllowCrossOriginEventReportingFromHeader_NoObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  EXPECT_FALSE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

TEST(
    FenceEventReportingParserTest,
    ParseAllowCrossOriginEventReportingFromHeader_TrueValueObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Allow-Cross-Origin-Event-Reporting: ?1\r\n");
  EXPECT_TRUE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

TEST(
    FenceEventReportingParserTest,
    ParseAllowCrossOriginEventReportingFromHeader_FalseValueObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Allow-Cross-Origin-Event-Reporting: ?0\r\n");
  EXPECT_FALSE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

TEST(
    FenceEventReportingParserTest,
    ParseAllowCrossOriginEventReportingFromHeader_NotBooleanObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Allow-Cross-Origin-Event-Reporting: 1\r\n");
  EXPECT_FALSE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

TEST(FenceEventReportingParserTest,
     ParseAllowCrossOriginEventReportingFromHeader_InvalidObserveTopicsHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Allow-Cross-Origin-Event-Reporting: !!!\r\n");
  EXPECT_FALSE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

TEST(FenceEventReportingParserTest,
     ParseAllowCrossOriginEventReportingFromHeader_InvalidNormalizedHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\n"
          "Allow-Cross-Origin-Event-Reporting: ?1\r\n"
          "Allow-Cross-Origin-Event-Reporting: ?1\r\n");
  EXPECT_FALSE(ParseAllowCrossOriginEventReportingFromHeader(*headers));
}

}  // namespace network
