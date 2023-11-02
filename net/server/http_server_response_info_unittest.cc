// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_status_code.h"
#include "net/server/http_server_response_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpServerResponseInfoTest, StatusLine) {
  HttpServerResponseInfo response;
  ASSERT_EQ(HTTP_OK, response.status_code());
  ASSERT_EQ("HTTP/1.1 200 OK\r\n\r\n", response.Serialize());
}

TEST(HttpServerResponseInfoTest, Headers) {
  HttpServerResponseInfo response;
  response.AddHeader("A", "1");
  response.AddHeader("A", "2");
  ASSERT_EQ("HTTP/1.1 200 OK\r\nA:1\r\nA:2\r\n\r\n", response.Serialize());
}

TEST(HttpServerResponseInfoTest, Body) {
  HttpServerResponseInfo response;
  ASSERT_EQ(std::string(), response.body());
  response.SetBody("body", "type");
  ASSERT_EQ("body", response.body());
  ASSERT_EQ(
      "HTTP/1.1 200 OK\r\nContent-Length:4\r\nContent-Type:type\r\n\r\nbody",
      response.Serialize());
}

TEST(HttpServerResponseInfoTest, CreateFor404) {
  HttpServerResponseInfo response = HttpServerResponseInfo::CreateFor404();
  ASSERT_EQ(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length:0\r\nContent-Type:text/html\r\n\r\n",
      response.Serialize());
}

TEST(HttpServerResponseInfoTest, CreateFor500) {
  HttpServerResponseInfo response =
      HttpServerResponseInfo::CreateFor500("mess");
  ASSERT_EQ(
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Content-Length:4\r\nContent-Type:text/html\r\n\r\nmess",
      response.Serialize());
}

}  // namespace net
