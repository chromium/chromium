// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_request.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace net::test_server {

TEST(HttpRequestTest, ParseRequest) {
  HttpRequestParser parser;

  // Process request in chunks to check if the parser deals with border cases.
  // Also, check multi-line headers as well as multiple requests in the same
  // chunk. This basically should cover all the simplest border cases.
  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Host: localhost:1234\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Multi-line-header: abcd\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk(" efgh\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk(" ijkl\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  parser.ProcessChunk("Content-Length: 10\r\n\r\n");
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
  // Content data and another request in the same chunk (possible in http/1.1).
  parser.ProcessChunk("1234567890GET /another.html HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  // Fetch the first request and validate it.
  {
    std::unique_ptr<HttpRequest> request = parser.GetRequest();
    EXPECT_EQ("/foobar.html", request->relative_url);
    EXPECT_EQ("POST", request->method_string);
    EXPECT_EQ(METHOD_POST, request->method);
    EXPECT_EQ("1234567890", request->content);
    ASSERT_EQ(3u, request->headers.size());

    EXPECT_EQ(1u, request->headers.count("Host"));
    EXPECT_EQ(1u, request->headers.count("Multi-line-header"));
    EXPECT_EQ(1u, request->headers.count("Content-Length"));

    const char kExpectedAllHeaders[] =
        "POST /foobar.html HTTP/1.1\r\n"
        "Host: localhost:1234\r\n"
        "Multi-line-header: abcd\r\n"
        " efgh\r\n"
        " ijkl\r\n"
        "Content-Length: 10\r\n";
    EXPECT_EQ(kExpectedAllHeaders, request->all_headers);
    EXPECT_EQ("localhost:1234", request->headers["Host"]);
    EXPECT_EQ("abcd efgh ijkl", request->headers["Multi-line-header"]);
    EXPECT_EQ("10", request->headers["Content-Length"]);
  }

  // No other request available yet since we do not support multiple requests
  // per connection.
  EXPECT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
}

TEST(HttpRequestTest, ParseRequestWithEmptyBody) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  parser.ProcessChunk("Content-Length: 0\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("", request->content);
  EXPECT_TRUE(request->has_content);
  EXPECT_EQ(1u, request->headers.count("Content-Length"));
  EXPECT_EQ("0", request->headers["Content-Length"]);
}

TEST(HttpRequestTest, ParseRequestWithChunkedBody) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  parser.ProcessChunk("Transfer-Encoding: chunked\r\n\r\n");
  parser.ProcessChunk("5\r\nhello\r\n");
  parser.ProcessChunk("1\r\n \r\n");
  parser.ProcessChunk("5\r\nworld\r\n");
  parser.ProcessChunk("0\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("hello world", request->content);
  EXPECT_TRUE(request->has_content);
  EXPECT_EQ(1u, request->headers.count("Transfer-Encoding"));
  EXPECT_EQ("chunked", request->headers["Transfer-Encoding"]);
}

TEST(HttpRequestTest, ParseRequestWithChunkedBodySlow) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n");
  parser.ProcessChunk("Transfer-Encoding: chunked\r\n\r\n");
  std::string chunked_body = "5\r\nhello\r\n0\r\n\r\n";

  // Send one character at a time, and make the parser parse the request.
  for (size_t i = 0; i < chunked_body.size(); i++) {
    parser.ProcessChunk(chunked_body.substr(i, 1));
    // Except for the last pass, ParseRequest() should give WAITING.
    if (i != chunked_body.size() - 1) {
      ASSERT_EQ(HttpRequestParser::WAITING, parser.ParseRequest());
    }
  }
  // All chunked data has been sent, the last ParseRequest should give ACCEPTED.
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());
  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("hello", request->content);
  EXPECT_TRUE(request->has_content);
  EXPECT_EQ(1u, request->headers.count("Transfer-Encoding"));
  EXPECT_EQ("chunked", request->headers["Transfer-Encoding"]);
}

TEST(HttpRequestTest, ParseRequestWithoutBody) {
  HttpRequestParser parser;

  parser.ProcessChunk("POST /foobar.html HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("", request->content);
  EXPECT_FALSE(request->has_content);
}

TEST(HttpRequestTest, ParseGet) {
  HttpRequestParser parser;

  parser.ProcessChunk("GET /foobar.html HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("/foobar.html", request->relative_url);
  EXPECT_EQ("GET", request->method_string);
  EXPECT_EQ(METHOD_GET, request->method);
  EXPECT_EQ("", request->content);
  EXPECT_FALSE(request->has_content);
}

TEST(HttpRequestTest, ParseConnect) {
  HttpRequestParser parser;

  parser.ProcessChunk("CONNECT example.com:443 HTTP/1.1\r\n\r\n");
  ASSERT_EQ(HttpRequestParser::ACCEPTED, parser.ParseRequest());

  std::unique_ptr<HttpRequest> request = parser.GetRequest();
  EXPECT_EQ("example.com:443", request->relative_url);
  EXPECT_EQ("CONNECT", request->method_string);
  EXPECT_EQ(METHOD_CONNECT, request->method);
  EXPECT_EQ("", request->content);
  EXPECT_FALSE(request->has_content);
}

TEST(HttpRequestTest, GetURL) {
  HttpRequest request;
  request.relative_url = "/foobar.html?q=foo";
  request.base_url = GURL("https://127.0.0.1:8080");
  EXPECT_EQ("https://127.0.0.1:8080/foobar.html?q=foo",
            request.GetURL().spec());
}

TEST(HttpRequestTest, GetURLFallback) {
  HttpRequest request;
  request.relative_url = "/foobar.html?q=foo";
  EXPECT_EQ("http://localhost/foobar.html?q=foo", request.GetURL().spec());
}

}  // namespace net::test_server
