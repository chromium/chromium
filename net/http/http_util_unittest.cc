// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_util.h"

#include <algorithm>
#include <limits>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpUtilTest, IsSafeHeader) {
  static const char* const unsafe_headers[] = {
      "sec-",
      "sEc-",
      "sec-foo",
      "sEc-FoO",
      "proxy-",
      "pRoXy-",
      "proxy-foo",
      "pRoXy-FoO",
      "accept-charset",
      "accept-encoding",
      "access-control-request-headers",
      "access-control-request-method",
      "access-control-request-private-network",
      "connection",
      "content-length",
      "cookie",
      "cookie2",
      "date",
      "dnt",
      "expect",
      "host",
      "keep-alive",
      "origin",
      "referer",
      "set-cookie",
      "te",
      "trailer",
      "transfer-encoding",
      "upgrade",
      "user-agent",
      "via",
  };
  for (const auto* unsafe_header : unsafe_headers) {
    EXPECT_FALSE(HttpUtil::IsSafeHeader(unsafe_header, "")) << unsafe_header;
    EXPECT_FALSE(HttpUtil::IsSafeHeader(base::ToUpperASCII(unsafe_header), ""))
        << unsafe_header;
  }
  static const char* const safe_headers[] = {
      "foo",
      "x-",
      "x-foo",
      "content-disposition",
      "update",
      "accept-charseta",
      "accept_charset",
      "accept-encodinga",
      "accept_encoding",
      "access-control-request-headersa",
      "access-control-request-header",
      "access_control_request_header",
      "access-control-request-methoda",
      "access_control_request_method",
      "connectiona",
      "content-lengtha",
      "content_length",
      "content-transfer-encoding",
      "cookiea",
      "cookie2a",
      "cookie3",
      "content-transfer-encodinga",
      "content_transfer_encoding",
      "datea",
      "expecta",
      "hosta",
      "keep-alivea",
      "keep_alive",
      "origina",
      "referera",
      "referrer",
      "tea",
      "trailera",
      "transfer-encodinga",
      "transfer_encoding",
      "upgradea",
      "user-agenta",
      "user_agent",
      "viaa",
      // Following 3 headers are safe if there is no forbidden method in values.
      "x-http-method",
      "x-http-method-override",
      "x-method-override",
  };
  for (const auto* safe_header : safe_headers) {
    EXPECT_TRUE(HttpUtil::IsSafeHeader(safe_header, "")) << safe_header;
    EXPECT_TRUE(HttpUtil::IsSafeHeader(base::ToUpperASCII(safe_header), ""))
        << safe_header;
  }

  static const char* const disallowed_with_forbidden_methods_headers[] = {
      "x-http-method",
      "x-http-method-override",
      "x-method-override",
  };
  static const struct {
    const char* value;
    bool is_safe;
  } disallowed_values[] = {{"connect", false},
                           {"trace", false},
                           {"track", false},
                           {"CONNECT", false},
                           {"cOnnEcT", false},
                           {"get", true},
                           {"get,post", true},
                           {"get,connect", false},
                           {"get, connect", false},
                           {"get,connect ", false},
                           {"get,connect ,post", false},
                           {"get,,,,connect", false},
                           {"trace,get,PUT", false}};
  for (const auto* header : disallowed_with_forbidden_methods_headers) {
    for (const auto& test_case : disallowed_values) {
      EXPECT_EQ(test_case.is_safe,
                HttpUtil::IsSafeHeader(header, test_case.value))
          << header << ": " << test_case.value;
    }
  }
}

TEST(HttpUtilTest, HeadersIterator) {
  std::string headers = "foo: 1\t\r\nbar: hello world\r\nbaz: 3 \r\n";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("hello world"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("baz"), it.name());
  EXPECT_EQ(std::string("3"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_MalformedLine) {
  std::string headers = "foo: 1\n: 2\n3\nbar: 4";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("4"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_MalformedName) {
  std::string headers = "[ignore me] /: 3\r\n";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_MalformedNameFollowedByValidLine) {
  std::string headers = "[ignore me] /: 3\r\nbar: 4\n";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("4"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_AdvanceTo) {
  std::string headers = "foo: 1\r\n: 2\r\n3\r\nbar: 4";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");
  EXPECT_TRUE(it.AdvanceTo("foo"));
  EXPECT_EQ("foo", it.name());
  EXPECT_TRUE(it.AdvanceTo("bar"));
  EXPECT_EQ("bar", it.name());
  EXPECT_FALSE(it.AdvanceTo("blat"));
  EXPECT_FALSE(it.GetNext());  // should be at end of headers
}

TEST(HttpUtilTest, HeadersIterator_Reset) {
  std::string headers = "foo: 1\r\n: 2\r\n3\r\nbar: 4";
  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");
  // Search past "foo".
  EXPECT_TRUE(it.AdvanceTo("bar"));
  // Now try advancing to "foo".  This time it should fail since the iterator
  // position is past it.
  EXPECT_FALSE(it.AdvanceTo("foo"));
  it.Reset();
  // Now that we reset the iterator position, we should find 'foo'
  EXPECT_TRUE(it.AdvanceTo("foo"));
}

TEST(HttpUtilTest, ValuesIterator) {
  std::string values = " must-revalidate,   no-cache=\"foo, bar\"\t, private ";

  HttpUtil::ValuesIterator it(values, ',',
                              /*ignore_empty_values=*/true);

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ("must-revalidate", it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ("no-cache=\"foo, bar\"", it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ("private", it.value());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, ValuesIterator_EmptyValues) {
  std::string values = ", foopy , \t ,,,";

  HttpUtil::ValuesIterator it(values, ',', /*ignore_empty_values=*/true);
  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ("foopy", it.value());
  EXPECT_FALSE(it.GetNext());

  HttpUtil::ValuesIterator it_with_empty_values(values, ',',
                                                /*ignore_empty_values=*/false);
  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());

  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("foopy", it_with_empty_values.value());

  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());

  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());

  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());

  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());

  EXPECT_FALSE(it_with_empty_values.GetNext());
}

TEST(HttpUtilTest, ValuesIterator_Blanks) {
  std::string values = " \t ";

  HttpUtil::ValuesIterator it(values, ',', /*ignore_empty_values=*/true);
  EXPECT_FALSE(it.GetNext());

  HttpUtil::ValuesIterator it_with_empty_values(values, ',',
                                                /*ignore_empty_values=*/false);
  ASSERT_TRUE(it_with_empty_values.GetNext());
  EXPECT_EQ("", it_with_empty_values.value());
  EXPECT_FALSE(it_with_empty_values.GetNext());
}

TEST(HttpUtilTest, Unquote) {
  // Replace <backslash> " with ".
  EXPECT_STREQ("xyz\"abc", HttpUtil::Unquote("\"xyz\\\"abc\"").c_str());

  // Replace <backslash> <backslash> with <backslash>
  EXPECT_STREQ("xyz\\abc", HttpUtil::Unquote("\"xyz\\\\abc\"").c_str());
  EXPECT_STREQ("xyz\\\\\\abc",
               HttpUtil::Unquote("\"xyz\\\\\\\\\\\\abc\"").c_str());

  // Replace <backslash> X with X
  EXPECT_STREQ("xyzXabc", HttpUtil::Unquote("\"xyz\\Xabc\"").c_str());

  // Act as identity function on unquoted inputs.
  EXPECT_STREQ("X", HttpUtil::Unquote("X").c_str());
  EXPECT_STREQ("\"", HttpUtil::Unquote("\"").c_str());

  // Allow quotes in the middle of the input.
  EXPECT_STREQ("foo\"bar", HttpUtil::Unquote("\"foo\"bar\"").c_str());

  // Allow the final quote to be escaped.
  EXPECT_STREQ("foo", HttpUtil::Unquote("\"foo\\\"").c_str());
}

TEST(HttpUtilTest, StrictUnquote) {
  std::string out;

  // Replace <backslash> " with ".
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"xyz\\\"abc\"", &out));
  EXPECT_STREQ("xyz\"abc", out.c_str());

  // Replace <backslash> <backslash> with <backslash>.
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"xyz\\\\abc\"", &out));
  EXPECT_STREQ("xyz\\abc", out.c_str());
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"xyz\\\\\\\\\\\\abc\"", &out));
  EXPECT_STREQ("xyz\\\\\\abc", out.c_str());

  // Replace <backslash> X with X.
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"xyz\\Xabc\"", &out));
  EXPECT_STREQ("xyzXabc", out.c_str());

  // Empty quoted string.
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"\"", &out));
  EXPECT_STREQ("", out.c_str());

  // Return false on unquoted inputs.
  EXPECT_FALSE(HttpUtil::StrictUnquote("X", &out));
  EXPECT_FALSE(HttpUtil::StrictUnquote("", &out));

  // Return false on mismatched quotes.
  EXPECT_FALSE(HttpUtil::StrictUnquote("\"", &out));
  EXPECT_FALSE(HttpUtil::StrictUnquote("\"xyz", &out));
  EXPECT_FALSE(HttpUtil::StrictUnquote("\"abc'", &out));

  // Return false on escaped terminal quote.
  EXPECT_FALSE(HttpUtil::StrictUnquote("\"abc\\\"", &out));
  EXPECT_FALSE(HttpUtil::StrictUnquote("\"\\\"", &out));

  // Allow escaped backslash before terminal quote.
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"\\\\\"", &out));
  EXPECT_STREQ("\\", out.c_str());

  // Don't allow single quotes to act as quote marks.
  EXPECT_FALSE(HttpUtil::StrictUnquote("'x\"'", &out));
  EXPECT_TRUE(HttpUtil::StrictUnquote("\"x'\"", &out));
  EXPECT_STREQ("x'", out.c_str());
  EXPECT_FALSE(HttpUtil::StrictUnquote("''", &out));
}

TEST(HttpUtilTest, Quote) {
  EXPECT_STREQ("\"xyz\\\"abc\"", HttpUtil::Quote("xyz\"abc").c_str());

  // Replace <backslash> <backslash> with <backslash>
  EXPECT_STREQ("\"xyz\\\\abc\"", HttpUtil::Quote("xyz\\abc").c_str());

  // Replace <backslash> X with X
  EXPECT_STREQ("\"xyzXabc\"", HttpUtil::Quote("xyzXabc").c_str());
}

TEST(HttpUtilTest, LocateEndOfHeaders) {
  struct {
    const std::string_view input;
    size_t expected_result;
  } tests[] = {
      {"\r\n", std::string::npos},
      {"\n", std::string::npos},
      {"\r", std::string::npos},
      {"foo", std::string::npos},
      {"\r\n\r\n", 4},
      {"foo\r\nbar\r\n\r\n", 12},
      {"foo\nbar\n\n", 9},
      {"foo\r\nbar\r\n\r\njunk", 12},
      {"foo\nbar\n\njunk", 9},
      {"foo\nbar\n\r\njunk", 10},
      {"foo\nbar\r\n\njunk", 10},
  };
  for (const auto& test : tests) {
    size_t eoh = HttpUtil::LocateEndOfHeaders(base::as_byte_span(test.input));
    EXPECT_EQ(test.expected_result, eoh);
  }
}

TEST(HttpUtilTest, LocateEndOfAdditionalHeaders) {
  struct {
    const std::string_view input;
    size_t expected_result;
  } tests[] = {
      {"\r\n", 2},
      {"\n", 1},
      {"\r", std::string::npos},
      {"foo", std::string::npos},
      {"\r\n\r\n", 2},
      {"foo\r\nbar\r\n\r\n", 12},
      {"foo\nbar\n\n", 9},
      {"foo\r\nbar\r\n\r\njunk", 12},
      {"foo\nbar\n\njunk", 9},
      {"foo\nbar\n\r\njunk", 10},
      {"foo\nbar\r\n\njunk", 10},
  };
  for (const auto& test : tests) {
    size_t eoh =
        HttpUtil::LocateEndOfAdditionalHeaders(base::as_byte_span(test.input));
    EXPECT_EQ(test.expected_result, eoh);
  }
}
TEST(HttpUtilTest, AssembleRawHeaders) {
  // clang-format off
  struct {
    const char* const input;  // with '|' representing '\0'
    const char* const expected_result;  // with '\0' changed to '|'
  } tests[] = {
    { "HTTP/1.0 200 OK\r\nFoo: 1\r\nBar: 2\r\n\r\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },

    { "HTTP/1.0 200 OK\nFoo: 1\nBar: 2\n\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },

    // Valid line continuation (single SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (single HT).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\tcontinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (multiple SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "   continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (multiple HT).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\t\t\tcontinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (mixed HT, SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " \t \t continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid multi-line continuation
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " continuation1\n"
      "\tcontinuation2\n"
      "  continuation3\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation1 continuation2 continuation3|"
      "Bar: 2||"
    },

    // Continuation of quoted value.
    // This is different from what Firefox does, since it
    // will preserve the LWS.
    {
      "HTTP/1.0 200 OK\n"
      "Etag: \"34534-d3\n"
      "    134q\"\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Etag: \"34534-d3 134q\"|"
      "Bar: 2||"
    },

    // Valid multi-line continuation, full LWS lines
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "         \n"
      "\t\t\t\t\n"
      "\t  continuation\n"
      "Bar: 2\n\n",

      // One SP per continued line = 3.
      "HTTP/1.0 200 OK|"
      "Foo: 1   continuation|"
      "Bar: 2||"
    },

    // Valid multi-line continuation, all LWS
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "         \n"
      "\t\t\t\t\n"
      "\t  \n"
      "Bar: 2\n\n",

      // One SP per continued line = 3.
      "HTTP/1.0 200 OK|"
      "Foo: 1   |"
      "Bar: 2||"
    },

    // Valid line continuation (No value bytes in first line).
    {
      "HTTP/1.0 200 OK\n"
      "Foo:\n"
      " value\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: value|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0 200 OK\n"
      " Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      " Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0\n"
      " 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0|"
      " 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0 404\n"
      " Not Found\n"
      "Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0 404|"
      " Not Found|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Unterminated status line.
    {
      "HTTP/1.0 200 OK",

      "HTTP/1.0 200 OK||"
    },

    // Single terminated, with headers
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not terminated, with headers
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "Bar: 2",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (VT)
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\vInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "\vInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation (formfeed)
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\fInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "\fInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation -- can't continue header names.
    {
      "HTTP/1.0 200 OK\n"
      "Serv\n"
      " er: Apache\n"
      "\tInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Serv|"
      " er: Apache|"
      "\tInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation -- no value to continue.
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "garbage\n"
      "  not-a-continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "garbage|"
      "  not-a-continuation|"
      "Bar: 2||",
    },

    // Not a line continuation -- no valid name.
    {
      "HTTP/1.0 200 OK\n"
      ": 1\n"
      "  garbage\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      ": 1|"
      "  garbage|"
      "Bar: 2||",
    },

    // Not a line continuation -- no valid name (whitespace)
    {
      "HTTP/1.0 200 OK\n"
      "   : 1\n"
      "  garbage\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "   : 1|"
      "  garbage|"
      "Bar: 2||",
    },

    // Embed NULLs in the status line. They should not be understood
    // as line separators.
    {
      "HTTP/1.0 200 OK|Bar2:0|Baz2:1\r\nFoo: 1\r\nBar: 2\r\n\r\n",
      "HTTP/1.0 200 OKBar2:0Baz2:1|Foo: 1|Bar: 2||"
    },

    // Embed NULLs in a header line. They should not be understood as
    // line separators.
    {
      "HTTP/1.0 200 OK\nFoo: 1|Foo2: 3\nBar: 2\n\n",
      "HTTP/1.0 200 OK|Foo: 1Foo2: 3|Bar: 2||"
    },

    // The embedded NUL at the start of the line (before "Blah:") should not be
    // interpreted as LWS (as that would mistake it for a header line
    // continuation).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "|Blah: 3\n"
      "Bar: 2\n\n",
      "HTTP/1.0 200 OK|Foo: 1|Blah: 3|Bar: 2||"
    },
  };
  // clang-format on
  for (const auto& test : tests) {
    std::string input = test.input;
    std::replace(input.begin(), input.end(), '|', '\0');
    std::string raw = HttpUtil::AssembleRawHeaders(input);
    std::replace(raw.begin(), raw.end(), '\0', '|');
    EXPECT_EQ(test.expected_result, raw);
  }
}

// Test SpecForRequest().
TEST(HttpUtilTest, RequestUrlSanitize) {
  struct {
    const char* const url;
    const char* const expected_spec;
  } tests[] = {
    { // Check that #hash is removed.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
    },
    { // The reference may itself contain # -- strip all of it.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1/?query=1",
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
    },
    { // https scheme
      "https://www.google.com:78/foobar?query=1#hash",
      "https://www.google.com:78/foobar?query=1",
    },
    { // WebSocket's ws scheme
      "ws://www.google.com:78/foobar?query=1#hash",
      "ws://www.google.com:78/foobar?query=1",
    },
    { // WebSocket's wss scheme
      "wss://www.google.com:78/foobar?query=1#hash",
      "wss://www.google.com:78/foobar?query=1",
    }
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(i);

    GURL url(GURL(tests[i].url));
    std::string expected_spec(tests[i].expected_spec);

    EXPECT_EQ(expected_spec, HttpUtil::SpecForRequest(url));
  }
}

TEST(HttpUtilTest, GenerateAcceptLanguageHeader) {
  std::string header = HttpUtil::GenerateAcceptLanguageHeader("");
  EXPECT_TRUE(header.empty());

  header = HttpUtil::GenerateAcceptLanguageHeader("es");
  EXPECT_EQ(std::string("es"), header);

  header = HttpUtil::GenerateAcceptLanguageHeader("en-US,fr,de");
  EXPECT_EQ(std::string("en-US,fr;q=0.9,de;q=0.8"), header);

  header = HttpUtil::GenerateAcceptLanguageHeader("en-US,fr,de,ko,zh-CN,ja");
  EXPECT_EQ(
      std::string("en-US,fr;q=0.9,de;q=0.8,ko;q=0.7,zh-CN;q=0.6,ja;q=0.5"),
      header);
}

// HttpResponseHeadersTest.GetMimeType also tests ParseContentType.
TEST(HttpUtilTest, ParseContentType) {
  // clang-format off
  const struct {
    const char* const content_type;
    const char* const expected_mime_type;
    const char* const expected_charset;
    const bool expected_had_charset;
    const char* const expected_boundary;
  } tests[] = {
    { "text/html",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html;",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html; charset=utf-8",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // Parameter name is "charset ", not "charset".  See https://crbug.com/772834.
    { "text/html; charset =utf-8",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html; charset= utf-8",
      "text/html",
      "utf-8",
      true,
      ""
    },
    { "text/html; charset=utf-8 ",
      "text/html",
      "utf-8",
      true,
      ""
    },

    { "text/html; boundary=\"WebKit-ada-df-dsf-adsfadsfs\"",
      "text/html",
      "",
      false,
      "WebKit-ada-df-dsf-adsfadsfs"
    },
    // Parameter name is "boundary ", not "boundary".
    // See https://crbug.com/772834.
    { "text/html; boundary =\"WebKit-ada-df-dsf-adsfadsfs\"",
      "text/html",
      "",
      false,
      ""
    },
    // Parameter value includes leading space.  See https://crbug.com/772834.
    { "text/html; boundary= \"WebKit-ada-df-dsf-adsfadsfs\"",
      "text/html",
      "",
      false,
      "WebKit-ada-df-dsf-adsfadsfs"
    },
    // Parameter value includes leading space.  See https://crbug.com/772834.
    { "text/html; boundary= \"WebKit-ada-df-dsf-adsfadsfs\"   ",
      "text/html",
      "",
      false,
      "WebKit-ada-df-dsf-adsfadsfs"
    },
    { "text/html; boundary=\"WebKit-ada-df-dsf-adsfadsfs  \"",
      "text/html",
      "",
      false,
      "WebKit-ada-df-dsf-adsfadsfs"
    },
    { "text/html; boundary=WebKit-ada-df-dsf-adsfadsfs",
      "text/html",
      "",
      false,
      "WebKit-ada-df-dsf-adsfadsfs"
    },
    { "text/html; charset",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html; charset=",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html; charset= ",
      "text/html",
      "",
      false,
      ""
    },
    { "text/html; charset= ;",
      "text/html",
      "",
      false,
      ""
    },
    // Empty quoted strings are allowed.
    { "text/html; charset=\"\"",
      "text/html",
      "",
      true,
      ""
    },

    // Leading and trailing whitespace in quotes is trimmed.
    { "text/html; charset=\" \"",
      "text/html",
      "",
      true,
      ""
    },
    { "text/html; charset=\" foo \"",
      "text/html",
      "foo",
      true,
      ""
    },

    // With multiple values, should use the first one.
    { "text/html; charset=foo; charset=utf-8",
      "text/html",
      "foo",
      true,
      ""
    },
    { "text/html; charset; charset=; charset=utf-8",
      "text/html",
      "utf-8",
      true,
      ""
    },
    { "text/html; charset=utf-8; charset=; charset",
      "text/html",
      "utf-8",
      true,
      ""
    },
    { "text/html; boundary=foo; boundary=bar",
      "text/html",
      "",
      false,
      "foo"
    },

    // Stray quotes ignored.
    { "text/html; \"; \"\"; charset=utf-8",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // Non-leading quotes kept as-is.
    { "text/html; charset=u\"tf-8\"",
      "text/html",
      "u\"tf-8\"",
      true,
      ""
    },
    { "text/html; charset=\"utf-8\"",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // No closing quote.
    { "text/html; charset=\"utf-8",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // Check that \ is treated as an escape character.
    { "text/html; charset=\"\\utf\\-\\8\"",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // More interseting escape character test - test escaped backslash, escaped
    // quote, and backslash at end of input in unterminated quoted string.
    { "text/html; charset=\"\\\\\\\"\\",
      "text/html",
      "\\\"\\",
      true,
      ""
    },
    // Check quoted semicolon.
    { "text/html; charset=\";charset=utf-8;\"",
      "text/html",
      ";charset=utf-8;",
      true,
      ""
    },
    // Unclear if this one should just return utf-8 or not.
    { "text/html; charset= \"utf-8\"",
      "text/html",
      "utf-8",
      true,
      ""
    },
    // Regression test for https://crbug.com/772350:
    // Single quotes are not delimiters but must be treated as part of charset.
    { "text/html; charset='utf-8'",
      "text/html",
      "'utf-8'",
      true,
      ""
    },
    // Empty subtype should be accepted.
    { "text/",
      "text/",
      "",
      false,
      ""
    },
    // "*/*" is ignored unless it has params, or is not an exact match.
    { "*/*", "", "", false, "" },
    { "*/*; charset=utf-8", "*/*", "utf-8", true, "" },
    { "*/* ", "*/*", "", false, "" },
    // Regression test for https://crbug.com/1326529
    { "teXT/html", "text/html", "", false, ""},
    // TODO(abarth): Add more interesting test cases.
  };
  // clang-format on
  for (const auto& test : tests) {
    std::string mime_type;
    std::string charset;
    bool had_charset = false;
    std::string boundary;
    HttpUtil::ParseContentType(test.content_type, &mime_type, &charset,
                               &had_charset, &boundary);
    EXPECT_EQ(test.expected_mime_type, mime_type)
        << "content_type=" << test.content_type;
    EXPECT_EQ(test.expected_charset, charset)
        << "content_type=" << test.content_type;
    EXPECT_EQ(test.expected_had_charset, had_charset)
        << "content_type=" << test.content_type;
    EXPECT_EQ(test.expected_boundary, boundary)
        << "content_type=" << test.content_type;
  }
}

TEST(HttpUtilTest, ParseContentResetCharset) {
  std::string mime_type;
  std::string charset;
  bool had_charset = false;
  std::string boundary;

  // Set mime (capitalization should be ignored), but not charset.
  HttpUtil::ParseContentType("Text/Html", &mime_type, &charset, &had_charset,
                             &boundary);
  EXPECT_EQ("text/html", mime_type);
  EXPECT_EQ("", charset);
  EXPECT_FALSE(had_charset);

  // The same mime, add charset.
  HttpUtil::ParseContentType("tExt/hTml;charset=utf-8", &mime_type, &charset,
                             &had_charset, &boundary);
  EXPECT_EQ("text/html", mime_type);
  EXPECT_EQ("utf-8", charset);
  EXPECT_TRUE(had_charset);

  // The same mime (different capitalization), but no charset - should not clear
  // charset.
  HttpUtil::ParseContentType("teXt/htMl", &mime_type, &charset, &had_charset,
                             &boundary);
  EXPECT_EQ("text/html", mime_type);
  EXPECT_EQ("utf-8", charset);
  EXPECT_TRUE(had_charset);

  // A different mime will clear charset.
  HttpUtil::ParseContentType("texT/plaiN", &mime_type, &charset, &had_charset,
                             &boundary);
  EXPECT_EQ("text/plain", mime_type);
  EXPECT_EQ("", charset);
  EXPECT_TRUE(had_charset);
}

TEST(HttpUtilTest, ParseContentRangeHeader) {
  const struct {
    const char* const content_range_header_spec;
    bool expected_return_value;
    int64_t expected_first_byte_position;
    int64_t expected_last_byte_position;
    int64_t expected_instance_length;
  } tests[] = {
      {"", false, -1, -1, -1},
      {"megabytes 0-10/50", false, -1, -1, -1},
      {"0-10/50", false, -1, -1, -1},
      {"Bytes 0-50/51", true, 0, 50, 51},
      {"bytes 0-50/51", true, 0, 50, 51},
      {"bytes\t0-50/51", false, -1, -1, -1},
      {"    bytes 0-50/51", true, 0, 50, 51},
      {"    bytes    0    -   50  \t / \t51", true, 0, 50, 51},
      {"bytes 0\t-\t50\t/\t51\t", true, 0, 50, 51},
      {"  \tbytes\t\t\t 0\t-\t50\t/\t51\t", true, 0, 50, 51},
      {"\t   bytes \t  0    -   50   /   5   1", false, -1, -1, -1},
      {"\t   bytes \t  0    -   5 0   /   51", false, -1, -1, -1},
      {"bytes 50-0/51", false, -1, -1, -1},
      {"bytes * /*", false, -1, -1, -1},
      {"bytes *   /    *   ", false, -1, -1, -1},
      {"bytes 0-50/*", false, -1, -1, -1},
      {"bytes 0-50  /    * ", false, -1, -1, -1},
      {"bytes 0-10000000000/10000000001", true, 0, 10000000000ll,
       10000000001ll},
      {"bytes 0-10000000000/10000000000", false, -1, -1, -1},
      // 64 bit wraparound.
      {"bytes 0 - 9223372036854775807 / 100", false, -1, -1, -1},
      // 64 bit wraparound.
      {"bytes 0 - 100 / -9223372036854775808", false, -1, -1, -1},
      {"bytes */50", false, -1, -1, -1},
      {"bytes 0-50/10", false, -1, -1, -1},
      {"bytes 40-50/45", false, -1, -1, -1},
      {"bytes 0-50/-10", false, -1, -1, -1},
      {"bytes 0-0/1", true, 0, 0, 1},
      {"bytes 0-40000000000000000000/40000000000000000001", false, -1, -1, -1},
      {"bytes 1-/100", false, -1, -1, -1},
      {"bytes -/100", false, -1, -1, -1},
      {"bytes -1/100", false, -1, -1, -1},
      {"bytes 0-1233/*", false, -1, -1, -1},
      {"bytes -123 - -1/100", false, -1, -1, -1},
  };

  for (const auto& test : tests) {
    int64_t first_byte_position, last_byte_position, instance_length;
    EXPECT_EQ(test.expected_return_value,
              HttpUtil::ParseContentRangeHeaderFor206(
                  test.content_range_header_spec, &first_byte_position,
                  &last_byte_position, &instance_length))
        << test.content_range_header_spec;
    EXPECT_EQ(test.expected_first_byte_position, first_byte_position)
        << test.content_range_header_spec;
    EXPECT_EQ(test.expected_last_byte_position, last_byte_position)
        << test.content_range_header_spec;
    EXPECT_EQ(test.expected_instance_length, instance_length)
        << test.content_range_header_spec;
  }
}

TEST(HttpUtilTest, ParseRetryAfterHeader) {
  base::Time::Exploded now_exploded = {2014, 11, 4, 5, 22, 39, 30, 0};
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCExploded(now_exploded, &now));

  base::Time::Exploded later_exploded = {2015, 1, 5, 1, 12, 34, 56, 0};
  base::Time later;
  EXPECT_TRUE(base::Time::FromUTCExploded(later_exploded, &later));

  const struct {
    const char* retry_after_string;
    bool expected_return_value;
    base::TimeDelta expected_retry_after;
  } tests[] = {{"", false, base::TimeDelta()},
               {"-3", false, base::TimeDelta()},
               {"-2", false, base::TimeDelta()},
               {"-1", false, base::TimeDelta()},
               {"+0", false, base::TimeDelta()},
               {"+1", false, base::TimeDelta()},
               {"0", true, base::Seconds(0)},
               {"1", true, base::Seconds(1)},
               {"2", true, base::Seconds(2)},
               {"3", true, base::Seconds(3)},
               {"60", true, base::Seconds(60)},
               {"3600", true, base::Seconds(3600)},
               {"86400", true, base::Seconds(86400)},
               {"Thu, 1 Jan 2015 12:34:56 GMT", true, later - now},
               {"Mon, 1 Jan 1900 12:34:56 GMT", false, base::TimeDelta()}};

  for (size_t i = 0; i < std::size(tests); ++i) {
    base::TimeDelta retry_after;
    bool return_value = HttpUtil::ParseRetryAfterHeader(
        tests[i].retry_after_string, now, &retry_after);
    EXPECT_EQ(tests[i].expected_return_value, return_value)
        << "Test case " << i << ": expected " << tests[i].expected_return_value
        << " but got " << return_value << ".";
    if (tests[i].expected_return_value && return_value) {
      EXPECT_EQ(tests[i].expected_retry_after, retry_after)
          << "Test case " << i << ": expected "
          << tests[i].expected_retry_after.InSeconds() << "s but got "
          << retry_after.InSeconds() << "s.";
    }
  }
}

TEST(HttpUtilTest, TimeFormatHTTP) {
  constexpr base::Time::Exploded kTime = {.year = 2011,
                                          .month = 4,
                                          .day_of_week = 6,
                                          .day_of_month = 30,
                                          .hour = 22,
                                          .minute = 42,
                                          .second = 7};
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTime, &time));
  EXPECT_EQ("Sat, 30 Apr 2011 22:42:07 GMT", HttpUtil::TimeFormatHTTP(time));
}

namespace {
void CheckCurrentNameValuePair(HttpUtil::NameValuePairsIterator* parser,
                               bool expect_valid,
                               std::string expected_name,
                               std::string expected_value) {
  ASSERT_EQ(expect_valid, parser->valid());
  if (!expect_valid) {
    return;
  }

  // Let's make sure that this never changes (i.e., when a quoted value is
  // unquoted, it should be cached on the first calls and not regenerated
  // later).
  const std::string_view first_value = parser->value();

  ASSERT_EQ(expected_name, parser->name());
  ASSERT_EQ(expected_value, parser->value());

  // Make sure they didn't/don't change.
  ASSERT_TRUE(first_value.data() == parser->value().data());
  ASSERT_TRUE(first_value.length() == parser->value().length());
}

void CheckNextNameValuePair(HttpUtil::NameValuePairsIterator* parser,
                            bool expect_next,
                            bool expect_valid,
                            std::string expected_name,
                            std::string expected_value) {
  ASSERT_EQ(expect_next, parser->GetNext());
  ASSERT_EQ(expect_valid, parser->valid());
  if (!expect_next || !expect_valid) {
    return;
  }

  CheckCurrentNameValuePair(parser,
                            expect_valid,
                            expected_name,
                            expected_value);
}

void CheckInvalidNameValuePair(std::string valid_part,
                               std::string invalid_part) {
  std::string whole_string = valid_part + invalid_part;

  HttpUtil::NameValuePairsIterator valid_parser(valid_part, /*delimiter=*/';');
  HttpUtil::NameValuePairsIterator invalid_parser(whole_string,
                                                  /*delimiter=*/';');

  ASSERT_TRUE(valid_parser.valid());
  ASSERT_TRUE(invalid_parser.valid());

  // Both parsers should return all the same values until "valid_parser" is
  // exhausted.
  while (valid_parser.GetNext()) {
    ASSERT_TRUE(invalid_parser.GetNext());
    ASSERT_TRUE(valid_parser.valid());
    ASSERT_TRUE(invalid_parser.valid());
    ASSERT_EQ(valid_parser.name(), invalid_parser.name());
    ASSERT_EQ(valid_parser.value(), invalid_parser.value());
  }

  // valid_parser is exhausted and remains 'valid'
  ASSERT_TRUE(valid_parser.valid());
  // But all data in it should have been cleared.
  EXPECT_TRUE(valid_parser.name().empty());
  EXPECT_TRUE(valid_parser.value().empty());
  EXPECT_TRUE(valid_parser.raw_value().empty());
  EXPECT_FALSE(valid_parser.value_is_quoted());

  // invalid_parser's corresponding call to GetNext also returns false...
  ASSERT_FALSE(invalid_parser.GetNext());
  // ...but the parser is in an invalid state.
  ASSERT_FALSE(invalid_parser.valid());

  // All values in an invalid parser should be cleared.
  EXPECT_TRUE(invalid_parser.name().empty());
  EXPECT_TRUE(invalid_parser.value().empty());
  EXPECT_TRUE(invalid_parser.raw_value().empty());
  EXPECT_FALSE(invalid_parser.value_is_quoted());
}

}  // namespace

TEST(HttpUtilTest, NameValuePairsIteratorCopyAndAssign) {
  std::string data =
      "alpha=\"\\\"a\\\"\"; beta=\" b \"; cappa=\"c;\"; delta=\"d\"";
  HttpUtil::NameValuePairsIterator parser_a(data, /*delimiter=*/';');

  EXPECT_TRUE(parser_a.valid());
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_a, true, true, "alpha", "\"a\""));

  HttpUtil::NameValuePairsIterator parser_b(parser_a);
  // a and b now point to same location
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "alpha", "\"a\""));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "alpha", "\"a\""));

  // advance a, no effect on b
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_a, true, true, "beta", " b "));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "alpha", "\"a\""));

  // assign b the current state of a, no effect on a
  parser_b = parser_a;
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "beta", " b "));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "beta", " b "));

  // advance b, no effect on a
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_b, true, true, "cappa", "c;"));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "beta", " b "));
}

TEST(HttpUtilTest, NameValuePairsIteratorEmptyInput) {
  std::string data;
  HttpUtil::NameValuePairsIterator parser(data, /*delimiter=*/';');

  EXPECT_TRUE(parser.valid());
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(
      &parser, false, true, std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIterator) {
  std::string data =
      "alpha=1; beta= 2 ;"
      "cappa =' 3; foo=';"
      "cappa =\" 3; foo=\";"
      "delta= \" \\\"4\\\" \"; e= \" '5'\"; e=6;"
      "f=\"\\\"\\h\\e\\l\\l\\o\\ \\w\\o\\r\\l\\d\\\"\";"
      "g=\"\"; h=\"hello\"";
  HttpUtil::NameValuePairsIterator parser(data, /*delimiter=*/';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "beta", "2"));

  // Single quotes shouldn't be treated as quotes.
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", "' 3"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "foo", "'"));

  // But double quotes should be, and can contain semi-colons and equal signs.
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", " 3; foo="));

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "delta", " \"4\" "));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "e", " '5'"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "e", "6"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "f", "\"hello world\""));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "g", std::string()));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "h", "hello"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(
      &parser, false, true, std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIteratorOptionalValues) {
  std::string data = "alpha=1; beta;cappa ;  delta; e    ; f=1";
  // Test that the default parser requires values.
  HttpUtil::NameValuePairsIterator default_parser(data, /*delimiter=*/';');
  EXPECT_TRUE(default_parser.valid());
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&default_parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&default_parser, false, false,
                                                 std::string(), std::string()));

  HttpUtil::NameValuePairsIterator values_required_parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::NOT_STRICT);
  EXPECT_TRUE(values_required_parser.valid());
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&values_required_parser, true,
                                                 true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(
      &values_required_parser, false, false, std::string(), std::string()));

  HttpUtil::NameValuePairsIterator parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::NOT_REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::NOT_STRICT);
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "beta", std::string()));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", std::string()));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "delta", std::string()));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "e", std::string()));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "f", "1"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&parser, false, true,
                                                 std::string(), std::string()));
  EXPECT_TRUE(parser.valid());
}

TEST(HttpUtilTest, NameValuePairsIteratorIllegalInputs) {
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", "; beta"));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair(std::string(), "beta"));

  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", "; \"beta\"=2"));
  ASSERT_NO_FATAL_FAILURE(
      CheckInvalidNameValuePair(std::string(), "\"beta\"=2"));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", ";beta="));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1",
                                                    ";beta=;cappa=2"));

  // According to the spec this is an error, but it doesn't seem appropriate to
  // change our behaviour to be less permissive at this time.
  // See NameValuePairsIteratorExtraSeparators test
  // ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", ";; beta=2"));
}

// If we are going to support extra separators against the spec, let's just make
// sure they work rationally.
TEST(HttpUtilTest, NameValuePairsIteratorExtraSeparators) {
  std::string data = " ; ;;alpha=1; ;; ; beta= 2;cappa=3;;; ; ";
  HttpUtil::NameValuePairsIterator parser(data, /*delimiter=*/';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "beta", "2"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", "3"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(
      &parser, false, true, std::string(), std::string()));
}

// See comments on the implementation of NameValuePairsIterator::GetNext
// regarding this derogation from the spec.
TEST(HttpUtilTest, NameValuePairsIteratorMissingEndQuote) {
  std::string data = "name=\"value";
  HttpUtil::NameValuePairsIterator parser(data, /*delimiter=*/';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "name", "value"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(
      &parser, false, true, std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIteratorStrictQuotesEscapedEndQuote) {
  std::string data = "foo=bar; name=\"value\\\"";
  HttpUtil::NameValuePairsIterator parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "foo", "bar"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&parser, false, false,
                                                 std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIteratorStrictQuotesQuoteInValue) {
  std::string data = "foo=\"bar\"; name=\"va\"lue\"";
  HttpUtil::NameValuePairsIterator parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "foo", "bar"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&parser, false, false,
                                                 std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIteratorStrictQuotesMissingEndQuote) {
  std::string data = "foo=\"bar\"; name=\"value";
  HttpUtil::NameValuePairsIterator parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "foo", "bar"));
  ASSERT_NO_FATAL_FAILURE(CheckNextNameValuePair(&parser, false, false,
                                                 std::string(), std::string()));
}

TEST(HttpUtilTest, NameValuePairsIteratorStrictQuotesSingleQuotes) {
  std::string data = "foo=\"bar\"; name='value; ok=it'";
  HttpUtil::NameValuePairsIterator parser(
      data, /*delimiter=*/';',
      HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "foo", "bar"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "name", "'value"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "ok", "it'"));
}

TEST(HttpUtilTest, HasValidators) {
  const char* const kMissing = "";
  const char* const kEtagEmpty = "\"\"";
  const char* const kEtagStrong = "\"strong\"";
  const char* const kEtagWeak = "W/\"weak\"";
  const char* const kLastModified = "Tue, 15 Nov 1994 12:45:26 GMT";
  const char* const kLastModifiedInvalid = "invalid";

  const HttpVersion v0_9 = HttpVersion(0, 9);
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kMissing, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagStrong, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagWeak, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagEmpty, kMissing));

  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kMissing, kLastModified));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagStrong, kLastModified));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagWeak, kLastModified));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagEmpty, kLastModified));

  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kMissing, kLastModifiedInvalid));
  EXPECT_FALSE(
      HttpUtil::HasValidators(v0_9, kEtagStrong, kLastModifiedInvalid));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagWeak, kLastModifiedInvalid));
  EXPECT_FALSE(HttpUtil::HasValidators(v0_9, kEtagEmpty, kLastModifiedInvalid));

  const HttpVersion v1_0 = HttpVersion(1, 0);
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kMissing, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kEtagStrong, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kEtagWeak, kMissing));
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kEtagEmpty, kMissing));

  EXPECT_TRUE(HttpUtil::HasValidators(v1_0, kMissing, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_0, kEtagStrong, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_0, kEtagWeak, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_0, kEtagEmpty, kLastModified));

  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kMissing, kLastModifiedInvalid));
  EXPECT_FALSE(
      HttpUtil::HasValidators(v1_0, kEtagStrong, kLastModifiedInvalid));
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kEtagWeak, kLastModifiedInvalid));
  EXPECT_FALSE(HttpUtil::HasValidators(v1_0, kEtagEmpty, kLastModifiedInvalid));

  const HttpVersion v1_1 = HttpVersion(1, 1);
  EXPECT_FALSE(HttpUtil::HasValidators(v1_1, kMissing, kMissing));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagStrong, kMissing));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagWeak, kMissing));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagEmpty, kMissing));

  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kMissing, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagStrong, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagWeak, kLastModified));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagEmpty, kLastModified));

  EXPECT_FALSE(HttpUtil::HasValidators(v1_1, kMissing, kLastModifiedInvalid));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagStrong, kLastModifiedInvalid));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagWeak, kLastModifiedInvalid));
  EXPECT_TRUE(HttpUtil::HasValidators(v1_1, kEtagEmpty, kLastModifiedInvalid));
}

TEST(HttpUtilTest, IsValidHeaderValue) {
  const char* const invalid_values[] = {
      "X-Requested-With: chrome${NUL}Sec-Unsafe: injected",
      "X-Requested-With: chrome\r\nSec-Unsafe: injected",
      "X-Requested-With: chrome\nSec-Unsafe: injected",
      "X-Requested-With: chrome\rSec-Unsafe: injected",
  };
  for (const std::string& value : invalid_values) {
    std::string replaced = value;
    base::ReplaceSubstringsAfterOffset(&replaced, 0, "${NUL}",
                                       std::string(1, '\0'));
    EXPECT_FALSE(HttpUtil::IsValidHeaderValue(replaced)) << replaced;
  }

  // Check that all characters permitted by RFC7230 3.2.6 are allowed.
  std::string allowed = "\t";
  for (char c = '\x20'; c < '\x7F'; ++c) {
    allowed.append(1, c);
  }
  for (int c = 0x80; c <= 0xFF; ++c) {
    allowed.append(1, static_cast<char>(c));
  }
  EXPECT_TRUE(HttpUtil::IsValidHeaderValue(allowed));
}

TEST(HttpUtilTest, IsToken) {
  EXPECT_TRUE(HttpUtil::IsToken("valid"));
  EXPECT_TRUE(HttpUtil::IsToken("!"));
  EXPECT_TRUE(HttpUtil::IsToken("~"));

  EXPECT_FALSE(HttpUtil::IsToken(""));
  EXPECT_FALSE(HttpUtil::IsToken(std::string_view()));
  EXPECT_FALSE(HttpUtil::IsToken("hello, world"));
  EXPECT_FALSE(HttpUtil::IsToken(" "));
  EXPECT_FALSE(HttpUtil::IsToken(std::string_view("\0", 1)));
  EXPECT_FALSE(HttpUtil::IsToken("\x01"));
  EXPECT_FALSE(HttpUtil::IsToken("\x7F"));
  EXPECT_FALSE(HttpUtil::IsToken("\x80"));
  EXPECT_FALSE(HttpUtil::IsToken("\xff"));
}

TEST(HttpUtilTest, IsLWS) {
  EXPECT_FALSE(HttpUtil::IsLWS('\v'));
  EXPECT_FALSE(HttpUtil::IsLWS('\0'));
  EXPECT_FALSE(HttpUtil::IsLWS('1'));
  EXPECT_FALSE(HttpUtil::IsLWS('a'));
  EXPECT_FALSE(HttpUtil::IsLWS('.'));
  EXPECT_FALSE(HttpUtil::IsLWS('\n'));
  EXPECT_FALSE(HttpUtil::IsLWS('\r'));

  EXPECT_TRUE(HttpUtil::IsLWS('\t'));
  EXPECT_TRUE(HttpUtil::IsLWS(' '));
}

TEST(HttpUtilTest, IsControlChar) {
  EXPECT_FALSE(HttpUtil::IsControlChar('1'));
  EXPECT_FALSE(HttpUtil::IsControlChar('a'));
  EXPECT_FALSE(HttpUtil::IsControlChar('.'));
  EXPECT_FALSE(HttpUtil::IsControlChar('$'));
  EXPECT_FALSE(HttpUtil::IsControlChar('\x7E'));
  EXPECT_FALSE(HttpUtil::IsControlChar('\x80'));
  EXPECT_FALSE(HttpUtil::IsControlChar('\xFF'));

  EXPECT_TRUE(HttpUtil::IsControlChar('\0'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\v'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\n'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\r'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\t'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\x01'));
  EXPECT_TRUE(HttpUtil::IsControlChar('\x7F'));
}

TEST(HttpUtilTest, ParseAcceptEncoding) {
  const struct {
    const char* const value;
    const char* const expected;
  } tests[] = {
      {"", "*"},
      {"identity;q=1, *;q=0", "identity"},
      {"identity", "identity"},
      {"FOO, Bar", "bar|foo|identity"},
      {"foo; q=1", "foo|identity"},
      {"abc, foo; Q=1.0", "abc|foo|identity"},
      {"abc, foo;q= 1.00 , bar", "abc|bar|foo|identity"},
      {"abc, foo; q=1.000, bar", "abc|bar|foo|identity"},
      {"abc, foo ; q = 0 , bar", "abc|bar|identity"},
      {"abc, foo; q=0.0, bar", "abc|bar|identity"},
      {"abc, foo; q=0.00, bar", "abc|bar|identity"},
      {"abc, foo; q=0.000, bar", "abc|bar|identity"},
      {"abc, foo; q=0.001, bar", "abc|bar|foo|identity"},
      {"gzip", "gzip|identity|x-gzip"},
      {"x-gzip", "gzip|identity|x-gzip"},
      {"compress", "compress|identity|x-compress"},
      {"x-compress", "compress|identity|x-compress"},
      {"x-compress", "compress|identity|x-compress"},
      {"foo bar", "INVALID"},
      {"foo;", "INVALID"},
      {"foo;w=1", "INVALID"},
      {"foo;q+1", "INVALID"},
      {"foo;q=2", "INVALID"},
      {"foo;q=1.001", "INVALID"},
      {"foo;q=0.", "INVALID"},
      {"foo,\"bar\"", "INVALID"},
  };

  for (const auto& test : tests) {
    std::string value(test.value);
    std::string reformatted;
    std::set<std::string> allowed_encodings;
    if (!HttpUtil::ParseAcceptEncoding(value, &allowed_encodings)) {
      reformatted = "INVALID";
    } else {
      std::vector<std::string> encodings_list;
      for (auto const& encoding : allowed_encodings)
        encodings_list.push_back(encoding);
      reformatted = base::JoinString(encodings_list, "|");
    }
    EXPECT_STREQ(test.expected, reformatted.c_str())
        << "value=\"" << value << "\"";
  }
}

TEST(HttpUtilTest, ParseContentEncoding) {
  const struct {
    const char* const value;
    const char* const expected;
  } tests[] = {
      {"", ""},
      {"identity;q=1, *;q=0", "INVALID"},
      {"identity", "identity"},
      {"FOO, zergli , Bar", "bar|foo|zergli"},
      {"foo, *", "INVALID"},
      {"foo,\"bar\"", "INVALID"},
  };

  for (const auto& test : tests) {
    std::string value(test.value);
    std::string reformatted;
    std::set<std::string> used_encodings;
    if (!HttpUtil::ParseContentEncoding(value, &used_encodings)) {
      reformatted = "INVALID";
    } else {
      std::vector<std::string> encodings_list;
      for (auto const& encoding : used_encodings)
        encodings_list.push_back(encoding);
      reformatted = base::JoinString(encodings_list, "|");
    }
    EXPECT_STREQ(test.expected, reformatted.c_str())
        << "value=\"" << value << "\"";
  }
}

// Test the expansion of the Language List.
TEST(HttpUtilTest, ExpandLanguageList) {
  EXPECT_EQ("", HttpUtil::ExpandLanguageList(""));
  EXPECT_EQ("en-US,en", HttpUtil::ExpandLanguageList("en-US"));
  EXPECT_EQ("fr", HttpUtil::ExpandLanguageList("fr"));

  // The base language is added after all regional codes...
  EXPECT_EQ("en-US,en-CA,en", HttpUtil::ExpandLanguageList("en-US,en-CA"));

  // ... but before other language families.
  EXPECT_EQ("en-US,en-CA,en,fr",
            HttpUtil::ExpandLanguageList("en-US,en-CA,fr"));
  EXPECT_EQ("en-US,en-CA,en,fr,en-AU",
            HttpUtil::ExpandLanguageList("en-US,en-CA,fr,en-AU"));
  EXPECT_EQ("en-US,en-CA,en,fr-CA,fr",
            HttpUtil::ExpandLanguageList("en-US,en-CA,fr-CA"));

  // Add a base language even if it's already in the list.
  EXPECT_EQ("en-US,en,fr-CA,fr,it,es-AR,es,it-IT",
            HttpUtil::ExpandLanguageList("en-US,fr-CA,it,fr,es-AR,it-IT"));
  // Trims a whitespace.
  EXPECT_EQ("en-US,en,fr", HttpUtil::ExpandLanguageList("en-US, fr"));

  // Do not expand the single character subtag 'x' as a language.
  EXPECT_EQ("x-private-agreement-subtags",
            HttpUtil::ExpandLanguageList("x-private-agreement-subtags"));
  // Do not expand the single character subtag 'i' as a language.
  EXPECT_EQ("i-klingon", HttpUtil::ExpandLanguageList("i-klingon"));
}

}  // namespace net
