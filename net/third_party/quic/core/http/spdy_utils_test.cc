// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

using spdy::SpdyHeaderBlock;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace quic {
namespace test {

static std::unique_ptr<QuicHeaderList> FromList(
    const QuicHeaderList::ListType& src) {
  std::unique_ptr<QuicHeaderList> headers(new QuicHeaderList);
  headers->OnHeaderBlockStart();
  for (const auto& p : src) {
    headers->OnHeader(p.first, p.second);
  }
  headers->OnHeaderBlockEnd(0, 0);
  return headers;
}

using CopyAndValidateHeaders = QuicTest;

TEST_F(CopyAndValidateHeaders, NormalUsage) {
  auto headers = FromList({// All cookie crumbs are joined.
                           {"cookie", " part 1"},
                           {"cookie", "part 2 "},
                           {"cookie", "part3"},

                           // Already-delimited headers are passed through.
                           {"passed-through", QuicString("foo\0baz", 7)},

                           // Other headers are joined on \0.
                           {"joined", "value 1"},
                           {"joined", "value 2"},

                           // Empty headers remain empty.
                           {"empty", ""},

                           // Joined empty headers work as expected.
                           {"empty-joined", ""},
                           {"empty-joined", "foo"},
                           {"empty-joined", ""},
                           {"empty-joined", ""},

                           // Non-continguous cookie crumb.
                           {"cookie", " fin!"}});

  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block,
              UnorderedElementsAre(
                  Pair("cookie", " part 1; part 2 ; part3;  fin!"),
                  Pair("passed-through", QuicStringPiece("foo\0baz", 7)),
                  Pair("joined", QuicStringPiece("value 1\0value 2", 15)),
                  Pair("empty", ""),
                  Pair("empty-joined", QuicStringPiece("\0foo\0\0", 6))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, EmptyName) {
  auto headers = FromList({{"foo", "foovalue"}, {"", "barvalue"}, {"baz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, UpperCaseName) {
  auto headers =
      FromList({{"foo", "foovalue"}, {"bar", "barvalue"}, {"bAz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, MultipleContentLengths) {
  auto headers = FromList({{"content-length", "9"},
                           {"foo", "foovalue"},
                           {"content-length", "9"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("content-length", QuicStringPiece("9\09", 3)),
                         Pair("baz", "")));
  EXPECT_EQ(9, content_length);
}

TEST_F(CopyAndValidateHeaders, InconsistentContentLengths) {
  auto headers = FromList({{"content-length", "9"},
                           {"foo", "foovalue"},
                           {"content-length", "8"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_FALSE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
}

TEST_F(CopyAndValidateHeaders, LargeContentLength) {
  auto headers = FromList({{"content-length", "9000000000"},
                           {"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"baz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("content-length", QuicStringPiece("9000000000")),
                         Pair("baz", "")));
  EXPECT_EQ(9000000000, content_length);
}

TEST_F(CopyAndValidateHeaders, MultipleValues) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"baz", ""},
                           {"foo", "boo"},
                           {"baz", "buzz"}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", QuicStringPiece("foovalue\0boo", 12)),
                         Pair("bar", "barvalue"),
                         Pair("baz", QuicStringPiece("\0buzz", 5))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, MoreThanTwoValues) {
  auto headers = FromList({{"set-cookie", "value1"},
                           {"set-cookie", "value2"},
                           {"set-cookie", "value3"}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(
      block, UnorderedElementsAre(Pair(
                 "set-cookie", QuicStringPiece("value1\0value2\0value3", 20))));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, Cookie) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"cookie", "value1"},
                           {"baz", ""}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("cookie", "value1"), Pair("baz", "")));
  EXPECT_EQ(-1, content_length);
}

TEST_F(CopyAndValidateHeaders, MultipleCookies) {
  auto headers = FromList({{"foo", "foovalue"},
                           {"bar", "barvalue"},
                           {"cookie", "value1"},
                           {"baz", ""},
                           {"cookie", "value2"}});
  int64_t content_length = -1;
  SpdyHeaderBlock block;
  ASSERT_TRUE(
      SpdyUtils::CopyAndValidateHeaders(*headers, &content_length, &block));
  EXPECT_THAT(block, UnorderedElementsAre(
                         Pair("foo", "foovalue"), Pair("bar", "barvalue"),
                         Pair("cookie", "value1; value2"), Pair("baz", "")));
  EXPECT_EQ(-1, content_length);
}

using CopyAndValidateTrailers = QuicTest;

TEST_F(CopyAndValidateTrailers, SimplestValidList) {
  // Verify that the simplest trailers are valid: just a final byte offset that
  // gets parsed successfully.
  auto trailers = FromList({{kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(*trailers, &final_byte_offset,
                                                 &block));
  EXPECT_EQ(1234u, final_byte_offset);
}

TEST_F(CopyAndValidateTrailers, EmptyTrailerList) {
  // An empty trailer list will fail as required key kFinalOffsetHeaderKey is
  // not present.
  QuicHeaderList trailers;
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_FALSE(
      SpdyUtils::CopyAndValidateTrailers(trailers, &final_byte_offset, &block));
}

TEST_F(CopyAndValidateTrailers, FinalByteOffsetNotPresent) {
  // Validation fails if required kFinalOffsetHeaderKey is not present, even if
  // the rest of the header block is valid.
  auto trailers = FromList({{"key", "value"}});
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(*trailers, &final_byte_offset,
                                                  &block));
}

TEST_F(CopyAndValidateTrailers, EmptyName) {
  // Trailer validation will fail with an empty header key, in an otherwise
  // valid block of trailers.
  auto trailers = FromList({{"", "value"}, {kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(*trailers, &final_byte_offset,
                                                  &block));
}

TEST_F(CopyAndValidateTrailers, PseudoHeaderInTrailers) {
  // Pseudo headers are illegal in trailers.
  auto trailers =
      FromList({{":pseudo_key", "value"}, {kFinalOffsetHeaderKey, "1234"}});
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_FALSE(SpdyUtils::CopyAndValidateTrailers(*trailers, &final_byte_offset,
                                                  &block));
}

TEST_F(CopyAndValidateTrailers, DuplicateTrailers) {
  // Duplicate trailers are allowed, and their values are concatenated into a
  // single string delimted with '\0'. Some of the duplicate headers
  // deliberately have an empty value.
  auto trailers = FromList({{"key", "value0"},
                            {"key", "value1"},
                            {"key", ""},
                            {"key", ""},
                            {"key", "value2"},
                            {"key", ""},
                            {kFinalOffsetHeaderKey, "1234"},
                            {"other_key", "value"},
                            {"key", "non_contiguous_duplicate"}});
  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_TRUE(SpdyUtils::CopyAndValidateTrailers(*trailers, &final_byte_offset,
                                                 &block));
  EXPECT_THAT(
      block,
      UnorderedElementsAre(
          Pair("key",
               QuicStringPiece(
                   "value0\0value1\0\0\0value2\0\0non_contiguous_duplicate",
                   48)),
          Pair("other_key", "value")));
}

TEST_F(CopyAndValidateTrailers, DuplicateCookies) {
  // Duplicate cookie headers in trailers should be concatenated into a single
  //  "; " delimted string.
  auto headers = FromList({{"cookie", " part 1"},
                           {"cookie", "part 2 "},
                           {"cookie", "part3"},
                           {"key", "value"},
                           {kFinalOffsetHeaderKey, "1234"},
                           {"cookie", " non_contiguous_cookie!"}});

  size_t final_byte_offset = 0;
  SpdyHeaderBlock block;
  EXPECT_TRUE(
      SpdyUtils::CopyAndValidateTrailers(*headers, &final_byte_offset, &block));
  EXPECT_THAT(
      block,
      UnorderedElementsAre(
          Pair("cookie", " part 1; part 2 ; part3;  non_contiguous_cookie!"),
          Pair("key", "value")));
}

using GetPromisedUrlFromHeaders = QuicTest;

TEST_F(GetPromisedUrlFromHeaders, Basic) {
  SpdyHeaderBlock headers;
  headers[":method"] = "GET";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":scheme"] = "https";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":authority"] = "www.google.com";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":path"] = "/index.html";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers),
            "https://www.google.com/index.html");
  headers["key1"] = "value1";
  headers["key2"] = "value2";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers),
            "https://www.google.com/index.html");
}

TEST_F(GetPromisedUrlFromHeaders, Connect) {
  SpdyHeaderBlock headers;
  headers[":method"] = "CONNECT";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":authority"] = "www.google.com";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":scheme"] = "https";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
  headers[":path"] = "https";
  EXPECT_EQ(SpdyUtils::GetPromisedUrlFromHeaders(headers), "");
}

using GetPromisedHostNameFromHeaders = QuicTest;

TEST_F(GetPromisedHostNameFromHeaders, NormalUsage) {
  SpdyHeaderBlock headers;
  headers[":method"] = "GET";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers), "");
  headers[":scheme"] = "https";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers), "");
  headers[":authority"] = "www.google.com";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers), "");
  headers[":path"] = "/index.html";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers),
            "www.google.com");
  headers["key1"] = "value1";
  headers["key2"] = "value2";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers),
            "www.google.com");
  headers[":authority"] = "www.google.com:6666";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers),
            "www.google.com");
  headers[":authority"] = "192.168.1.1";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers), "192.168.1.1");
  headers[":authority"] = "192.168.1.1:6666";
  EXPECT_EQ(SpdyUtils::GetPromisedHostNameFromHeaders(headers), "192.168.1.1");
}

using PopulateHeaderBlockFromUrl = QuicTest;

TEST_F(PopulateHeaderBlockFromUrl, NormalUsage) {
  QuicString url = "https://www.google.com/index.html";
  SpdyHeaderBlock headers;
  EXPECT_TRUE(SpdyUtils::PopulateHeaderBlockFromUrl(url, &headers));
  EXPECT_EQ("https", headers[":scheme"].as_string());
  EXPECT_EQ("www.google.com", headers[":authority"].as_string());
  EXPECT_EQ("/index.html", headers[":path"].as_string());
}

TEST_F(PopulateHeaderBlockFromUrl, UrlWithNoPath) {
  QuicString url = "https://www.google.com";
  SpdyHeaderBlock headers;
  EXPECT_TRUE(SpdyUtils::PopulateHeaderBlockFromUrl(url, &headers));
  EXPECT_EQ("https", headers[":scheme"].as_string());
  EXPECT_EQ("www.google.com", headers[":authority"].as_string());
  EXPECT_EQ("/", headers[":path"].as_string());
}

TEST_F(PopulateHeaderBlockFromUrl, Failure) {
  SpdyHeaderBlock headers;
  EXPECT_FALSE(SpdyUtils::PopulateHeaderBlockFromUrl("/", &headers));
  EXPECT_FALSE(SpdyUtils::PopulateHeaderBlockFromUrl("/index.html", &headers));
  EXPECT_FALSE(
      SpdyUtils::PopulateHeaderBlockFromUrl("www.google.com/", &headers));
}

using PushPromiseUrlTest = QuicTest;

TEST_F(PushPromiseUrlTest, GetPushPromiseUrl) {
  // Test rejection of various inputs.
  EXPECT_EQ("",
            SpdyUtils::GetPushPromiseUrl("file", "localhost", "/etc/password"));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("file", "",
                                             "/C:/Windows/System32/Config/"));
  EXPECT_EQ("",
            SpdyUtils::GetPushPromiseUrl("", "https://www.google.com", "/"));

  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https://www.google.com",
                                             "www.google.com", "/"));
  EXPECT_EQ("",
            SpdyUtils::GetPushPromiseUrl("https://", "www.google.com", "/"));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https", "", "/"));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https", "", "www.google.com/"));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https", "www.google.com/", "/"));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https", "www.google.com", ""));
  EXPECT_EQ("", SpdyUtils::GetPushPromiseUrl("https", "www.google", ".com/"));

  // Test acception/rejection of various input combinations.
  // |input_headers| is an array of pairs. The first value of each pair is a
  // string that will be used as one of the inputs of GetPushPromiseUrl(). The
  // second value of each pair is a bitfield where the lowest 3 bits indicate
  // for which headers that string is valid (in a PUSH_PROMISE). For example,
  // the string "http" would be valid for both the ":scheme" and ":authority"
  // headers, so the bitfield paired with it is set to SCHEME | AUTH.
  const unsigned char SCHEME = (1u << 0);
  const unsigned char AUTH = (1u << 1);
  const unsigned char PATH = (1u << 2);
  const std::pair<const char*, unsigned char> input_headers[] = {
      {"http", SCHEME | AUTH},
      {"https", SCHEME | AUTH},
      {"hTtP", SCHEME | AUTH},
      {"HTTPS", SCHEME | AUTH},
      {"www.google.com", AUTH},
      {"90af90e0", AUTH},
      {"12foo%20-bar:00001233", AUTH},
      {"GOO\u200b\u2060\ufeffgoo", AUTH},
      {"192.168.0.5", AUTH},
      {"[::ffff:192.168.0.1.]", AUTH},
      {"http:", AUTH},
      {"bife l", AUTH},
      {"/", PATH},
      {"/foo/bar/baz", PATH},
      {"/%20-2DVdkj.cie/foe_.iif/", PATH},
      {"http://", 0},
      {":443", 0},
      {":80/eddd", 0},
      {"google.com:-0", 0},
      {"google.com:65536", 0},
      {"http://google.com", 0},
      {"http://google.com:39", 0},
      {"//google.com/foo", 0},
      {".com/", 0},
      {"http://www.google.com/", 0},
      {"http://foo:439", 0},
      {"[::ffff:192.168", 0},
      {"]/", 0},
      {"//", 0}};
  for (size_t i = 0; i < QUIC_ARRAYSIZE(input_headers); ++i) {
    bool should_accept = (input_headers[i].second & SCHEME);
    for (size_t j = 0; j < QUIC_ARRAYSIZE(input_headers); ++j) {
      bool should_accept_2 = should_accept && (input_headers[j].second & AUTH);
      for (size_t k = 0; k < QUIC_ARRAYSIZE(input_headers); ++k) {
        // |should_accept_3| indicates whether or not GetPushPromiseUrl() is
        // expected to accept this input combination.
        bool should_accept_3 =
            should_accept_2 && (input_headers[k].second & PATH);

        std::string url = SpdyUtils::GetPushPromiseUrl(input_headers[i].first,
                                                       input_headers[j].first,
                                                       input_headers[k].first);

        ::testing::AssertionResult result = ::testing::AssertionSuccess();
        if (url.empty() == should_accept_3) {
          result = ::testing::AssertionFailure()
                   << "GetPushPromiseUrl() accepted/rejected the inputs when "
                      "it shouldn't have."
                   << std::endl
                   << "     scheme: " << input_headers[i].first << std::endl
                   << "  authority: " << input_headers[j].first << std::endl
                   << "       path: " << input_headers[k].first << std::endl
                   << "Output: " << url << std::endl;
        }
        ASSERT_TRUE(result);
      }
    }
  }

  // Test canonicalization of various valid inputs.
  EXPECT_EQ("http://www.google.com/",
            SpdyUtils::GetPushPromiseUrl("http", "www.google.com", "/"));
  EXPECT_EQ(
      "https://www.goo-gle.com/fOOo/baRR",
      SpdyUtils::GetPushPromiseUrl("hTtPs", "wWw.gOo-gLE.cOm", "/fOOo/baRR"));
  EXPECT_EQ("https://www.goo-gle.com:3278/pAth/To/reSOurce",
            SpdyUtils::GetPushPromiseUrl("hTtPs", "Www.gOo-Gle.Com:000003278",
                                         "/pAth/To/reSOurce"));
  EXPECT_EQ("https://foo%20bar/foo/bar/baz",
            SpdyUtils::GetPushPromiseUrl("https", "foo bar", "/foo/bar/baz"));
  EXPECT_EQ("http://foo.com:70/e/",
            SpdyUtils::GetPushPromiseUrl("http", "foo.com:0000070", "/e/"));
  EXPECT_EQ(
      "http://192.168.0.1:70/e/",
      SpdyUtils::GetPushPromiseUrl("http", "0300.0250.00.01:0070", "/e/"));
  EXPECT_EQ("http://192.168.0.1/e/",
            SpdyUtils::GetPushPromiseUrl("http", "0xC0a80001", "/e/"));
  EXPECT_EQ("http://[::c0a8:1]/",
            SpdyUtils::GetPushPromiseUrl("http", "[::192.168.0.1]", "/"));
  EXPECT_EQ(
      "https://[::ffff:c0a8:1]/",
      SpdyUtils::GetPushPromiseUrl("https", "[::ffff:0xC0.0Xa8.0x0.0x1]", "/"));
}

}  // namespace test
}  // namespace quic
