/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Basic tests that verify our KURL's interface behaves the same as the
// original KURL's.

#include "third_party/blink/renderer/platform/weborigin/kurl.h"

#include <stdint.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/url_util.h"

namespace blink {

TEST(KURLTest, Getters) {
  struct GetterCase {
    const char* url;
    const char* protocol;
    const char* host;
    int port;
    const char* user;
    const char* pass;
    const char* path;
    const char* last_path_component;
    const char* query;
    const char* fragment_identifier;
    bool has_fragment_identifier;
  } cases[] = {
      {"http://www.google.com/foo/blah?bar=baz#ref", "http", "www.google.com",
       0, "", nullptr, "/foo/blah", "blah", "bar=baz", "ref", true},
      {// Non-ASCII code points in the fragment part. fragmentIdentifier()
       // should return it in percent-encoded form.
       "http://www.google.com/foo/blah?bar=baz#\xce\xb1\xce\xb2", "http",
       "www.google.com", 0, "", nullptr, "/foo/blah", "blah", "bar=baz",
       "%CE%B1%CE%B2", true},
      {"http://foo.com:1234/foo/bar/", "http", "foo.com", 1234, "", nullptr,
       "/foo/bar/", "bar", nullptr, nullptr, false},
      {"http://www.google.com?#", "http", "www.google.com", 0, "", nullptr, "/",
       nullptr, "", "", true},
      {"https://me:pass@google.com:23#foo", "https", "google.com", 23, "me",
       "pass", "/", nullptr, nullptr, "foo", true},
      {"javascript:hello!//world", "javascript", "", 0, "", nullptr,
       "hello!//world", "world", nullptr, nullptr, false},
      {// Recognize a query and a fragment in the path portion of a path
       // URL.
       "javascript:hello!?#/\\world", "javascript", "", 0, "", nullptr,
       "hello!", "hello!", "", "/\\world", true},
      {// lastPathComponent() method handles "parameters" in a path. path()
       // method doesn't.
       "http://a.com/hello;world", "http", "a.com", 0, "", nullptr,
       "/hello;world", "hello", nullptr, nullptr, false},
      {// IDNA
       "http://\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd/", "http",
       "xn--6qqa088eba", 0, "", nullptr, "/", nullptr, nullptr, nullptr, false},
  };

  for (size_t i = 0; i < base::size(cases); i++) {
    const GetterCase& c = cases[i];

    const String& url = String::FromUTF8(c.url);

    const KURL kurl(url);

    // Casted to the String (or coverted to using fromUTF8() for
    // expectations which may include non-ASCII code points) so that the
    // contents are printed on failure.
    EXPECT_EQ(String(c.protocol), kurl.Protocol()) << url;
    EXPECT_EQ(String(c.host), kurl.Host()) << url;
    EXPECT_EQ(c.port, kurl.Port()) << url;
    EXPECT_EQ(String(c.user), kurl.User()) << url;
    EXPECT_EQ(String(c.pass), kurl.Pass()) << url;
    EXPECT_EQ(String(c.path), kurl.GetPath()) << url;
    EXPECT_EQ(String(c.last_path_component), kurl.LastPathComponent()) << url;
    EXPECT_EQ(String(c.query), kurl.Query()) << url;
    if (c.has_fragment_identifier)
      EXPECT_EQ(String::FromUTF8(c.fragment_identifier),
                kurl.FragmentIdentifier())
          << url;
    else
      EXPECT_TRUE(kurl.FragmentIdentifier().IsNull()) << url;
  }
}

TEST(KURLTest, Setters) {
  // Replace the starting URL with the given components one at a time and
  // verify that we're always the same as the old KURL.
  //
  // Note that old KURL won't canonicalize the default port away, so we
  // can't set setting the http port to "80" (or even "0").
  //
  // We also can't test clearing the query.
  struct ExpectedComponentCase {
    const char* url;

    const char* protocol;
    const char* expected_protocol;

    const char* host;
    const char* expected_host;

    const int port;
    const char* expected_port;

    const char* user;
    const char* expected_user;

    const char* pass;
    const char* expected_pass;

    const char* path;
    const char* expected_path;

    const char* query;
    const char* expected_query;
  } cases[] = {
      {"http://www.google.com/",
       // protocol
       "https", "https://www.google.com/",
       // host
       "news.google.com", "https://news.google.com/",
       // port
       8888, "https://news.google.com:8888/",
       // user
       "me", "https://me@news.google.com:8888/",
       // pass
       "pass", "https://me:pass@news.google.com:8888/",
       // path
       "/foo", "https://me:pass@news.google.com:8888/foo",
       // query
       "?q=asdf", "https://me:pass@news.google.com:8888/foo?q=asdf"},
      {"https://me:pass@google.com:88/a?f#b",
       // protocol
       "http", "http://me:pass@google.com:88/a?f#b",
       // host
       "goo.com", "http://me:pass@goo.com:88/a?f#b",
       // port
       92, "http://me:pass@goo.com:92/a?f#b",
       // user
       "", "http://:pass@goo.com:92/a?f#b",
       // pass
       "", "http://goo.com:92/a?f#b",
       // path
       "/", "http://goo.com:92/?f#b",
       // query
       nullptr, "http://goo.com:92/#b"},
  };

  for (size_t i = 0; i < base::size(cases); i++) {
    KURL kurl(cases[i].url);

    kurl.SetProtocol(cases[i].protocol);
    EXPECT_EQ(cases[i].expected_protocol, kurl.GetString().Utf8());

    kurl.SetHost(cases[i].host);
    EXPECT_EQ(cases[i].expected_host, kurl.GetString().Utf8());

    kurl.SetPort(cases[i].port);
    EXPECT_EQ(cases[i].expected_port, kurl.GetString().Utf8());

    kurl.SetUser(cases[i].user);
    EXPECT_EQ(cases[i].expected_user, kurl.GetString().Utf8());

    kurl.SetPass(cases[i].pass);
    EXPECT_EQ(cases[i].expected_pass, kurl.GetString().Utf8());

    kurl.SetPath(cases[i].path);
    EXPECT_EQ(cases[i].expected_path, kurl.GetString().Utf8());

    kurl.SetQuery(cases[i].query);
    EXPECT_EQ(cases[i].expected_query, kurl.GetString().Utf8());

    // Refs are tested below. On the Safari 3.1 branch, we don't match their
    // KURL since we integrated a fix from their trunk.
  }
}

// Tests that KURL::decodeURLEscapeSequences works as expected
TEST(KURLTest, DecodeURLEscapeSequences) {
  struct DecodeCase {
    const char* input;
    const char* output;
  } decode_cases[] = {
      {"hello, world", "hello, world"},
      {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/",
       "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
      {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/",
       "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
      {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/",
       " !\"#$%&'()*+,-.//"},
      {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/",
       "0123456789:;<=>?/"},
      {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/",
       "@ABCDEFGHIJKLMNO/"},
      {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/",
       "PQRSTUVWXYZ[\\]^_/"},
      {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/",
       "`abcdefghijklmno/"},
      {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/",
       "pqrstuvwxyz{|}~\x7f/"},
      // Test un-UTF-8-ization.
      {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
  };

  for (size_t i = 0; i < base::size(decode_cases); i++) {
    String input(decode_cases[i].input);
    String str =
        DecodeURLEscapeSequences(input, DecodeURLMode::kUTF8OrIsomorphic);
    EXPECT_EQ(decode_cases[i].output, str.Utf8());
  }

  // Our decode should decode %00
  String zero =
      DecodeURLEscapeSequences("%00", DecodeURLMode::kUTF8OrIsomorphic);
  EXPECT_NE("%00", zero.Utf8());

  // Decode UTF-8.
  String decoded = DecodeURLEscapeSequences("%e6%bc%a2%e5%ad%97",
                                            DecodeURLMode::kUTF8OrIsomorphic);
  const UChar kDecodedExpected[] = {0x6F22, 0x5b57};
  EXPECT_EQ(String(kDecodedExpected, base::size(kDecodedExpected)), decoded);

  // Test the error behavior for invalid UTF-8 (we differ from WebKit here).
  // %e4 %a0 are invalid for UTF-8, but %e5%a5%bd is valid.
  String invalid = DecodeURLEscapeSequences("%e4%a0%e5%a5%bd",
                                            DecodeURLMode::kUTF8OrIsomorphic);
  UChar invalid_expected_helper[6] = {0x00e4, 0x00a0, 0x00e5,
                                      0x00a5, 0x00bd, 0};
  String invalid_expected(
      reinterpret_cast<const ::UChar*>(invalid_expected_helper), 5);
  EXPECT_EQ(invalid_expected, invalid);
}

TEST(KURLTest, EncodeWithURLEscapeSequences) {
  struct EncodeCase {
    const char* input;
    const char* output;
  } encode_cases[] = {
      {"hello, world", "hello%2C%20world"},
      {"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
       "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"},
      {"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F",
       "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"},
      {" !\"#$%&'()*+,-./", "%20!%22%23%24%25%26%27()*%2B%2C-./"},
      {"0123456789:;<=>?", "0123456789%3A%3B%3C%3D%3E%3F"},
      {"@ABCDEFGHIJKLMNO", "%40ABCDEFGHIJKLMNO"},
      {"PQRSTUVWXYZ[\\]^_", "PQRSTUVWXYZ%5B%5C%5D%5E_"},
      {"`abcdefghijklmno", "%60abcdefghijklmno"},
      {"pqrstuvwxyz{|}~\x7f", "pqrstuvwxyz%7B%7C%7D~%7F"},
  };

  for (size_t i = 0; i < base::size(encode_cases); i++) {
    String input(encode_cases[i].input);
    String expected_output(encode_cases[i].output);
    String output = EncodeWithURLEscapeSequences(input);
    EXPECT_EQ(expected_output, output);
  }

  // Our encode escapes NULLs for safety, so we need to check that too.
  String input("\x00\x01", 2u);
  String reference("%00%01");

  String output = EncodeWithURLEscapeSequences(input);
  EXPECT_EQ(reference, output);

  // Also test that it gets converted to UTF-8 properly.
  UChar wide_input_helper[3] = {0x4f60, 0x597d, 0};
  String wide_input(reinterpret_cast<const ::UChar*>(wide_input_helper), 2);
  String wide_reference("%E4%BD%A0%E5%A5%BD");
  String wide_output = EncodeWithURLEscapeSequences(wide_input);
  EXPECT_EQ(wide_reference, wide_output);

  // Encoding should not NFC-normalize the string.
  // Contain a combining character ('e' + COMBINING OGONEK).
  String combining(String::FromUTF8("\x65\xCC\xA8"));
  EXPECT_EQ(EncodeWithURLEscapeSequences(combining), "e%CC%A8");
  // Contain a precomposed character corresponding to |combining|.
  String precomposed(String::FromUTF8("\xC4\x99"));
  EXPECT_EQ(EncodeWithURLEscapeSequences(precomposed), "%C4%99");
}

TEST(KURLTest, AbsoluteRemoveWhitespace) {
  struct {
    const char* input;
    const char* expected;
  } cases[] = {
      {"ht\ntps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"ht\ttps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"ht\rtps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\nmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\tmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\rmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\nay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\tay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\ray?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\noo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\too#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\roo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\noo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\too", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\roo", "https://example.com/yay?boo#foo"},
  };

  for (const auto& test : cases) {
    const KURL input(test.input);
    const KURL expected(test.expected);
    EXPECT_EQ(input, expected);
  }
}

TEST(KURLTest, RelativeRemoveWhitespace) {
  struct {
    const char* base;
    const char* relative;
  } cases[] = {
      {"http://example.com/", "/path"},   {"http://example.com/", "\n/path"},
      {"http://example.com/", "\r/path"}, {"http://example.com/", "\t/path"},
      {"http://example.com/", "/pa\nth"}, {"http://example.com/", "/pa\rth"},
      {"http://example.com/", "/pa\tth"}, {"http://example.com/", "/path\n"},
      {"http://example.com/", "/path\r"}, {"http://example.com/", "/path\t"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.base << ", " << test.relative);
    const KURL base(test.base);
    const KURL expected("http://example.com/path");
    const KURL actual(base, test.relative);
    EXPECT_EQ(actual, expected);
  }
}

TEST(KURLTest, AbsolutePotentiallyDanglingMarkup) {
  struct {
    const char* input;
    const char* expected;
    const bool potentially_dangling_markup;
  } cases[] = {
      // Just removable whitespace isn't enough:
      {"ht\ntps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"ht\ttps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"ht\rtps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://exa\nmple.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://exa\tmple.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://exa\rmple.com/yay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/y\nay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/y\tay?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/y\ray?boo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?b\noo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?b\too#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?b\roo#foo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?boo#f\noo", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?boo#f\too", "https://example.com/yay?boo#foo",
       false},
      {"https://example.com/yay?boo#f\roo", "https://example.com/yay?boo#foo",
       false},

      // Likewise, just a brace won't cut it:
      {"https://example.com/y<ay?boo#foo", "https://example.com/y%3Cay?boo#foo",
       false},
      {"https://example.com/yay?b<oo#foo", "https://example.com/yay?b%3Coo#foo",
       false},
      {"https://example.com/yay?boo#f<oo", "https://example.com/yay?boo#f<oo",
       false},

      // Both, however:
      {"ht\ntps://example.com/y<ay?boo#foo",
       "https://example.com/y%3Cay?boo#foo", true},
      {"https://e\nxample.com/y<ay?boo#foo",
       "https://example.com/y%3Cay?boo#foo", true},
      {"https://example.com/y<\nay?boo#foo",
       "https://example.com/y%3Cay?boo#foo", true},
      {"https://example.com/y<ay?b\noo#foo",
       "https://example.com/y%3Cay?boo#foo", true},
      {"https://example.com/y<ay?boo#f\noo",
       "https://example.com/y%3Cay?boo#foo", true},
      {"ht\ntps://example.com/yay?b<oo#foo",
       "https://example.com/yay?b%3Coo#foo", true},
      {"https://e\nxample.com/yay?b<oo#foo",
       "https://example.com/yay?b%3Coo#foo", true},
      {"https://example.com/y\nay?b<oo#foo",
       "https://example.com/yay?b%3Coo#foo", true},
      {"https://example.com/yay?b<\noo#foo",
       "https://example.com/yay?b%3Coo#foo", true},
      {"https://example.com/yay?b<oo#f\noo",
       "https://example.com/yay?b%3Coo#foo", true},
      {"ht\ntps://example.com/yay?boo#f<oo", "https://example.com/yay?boo#f<oo",
       true},
      {"https://e\nxample.com/yay?boo#f<oo", "https://example.com/yay?boo#f<oo",
       true},
      {"https://example.com/y\nay?boo#f<oo", "https://example.com/yay?boo#f<oo",
       true},
      {"https://example.com/yay?b\noo#f<oo", "https://example.com/yay?boo#f<oo",
       true},
      {"https://example.com/yay?boo#f<\noo", "https://example.com/yay?boo#f<oo",
       true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.input << ", " << test.expected);
    const KURL input(test.input);
    const KURL expected(test.expected);
    EXPECT_EQ(input, expected) << input.GetString() << expected.GetString();
    EXPECT_EQ(test.potentially_dangling_markup,
              input.PotentiallyDanglingMarkup());
    EXPECT_FALSE(expected.PotentiallyDanglingMarkup());
  }
}

TEST(KURLTest, ResolveEmpty) {
  const KURL empty_base;

  // WebKit likes to be able to resolve absolute input agains empty base URLs,
  // which would normally be invalid since the base URL is invalid.
  const char kAbs[] = "http://www.google.com/";
  KURL resolve_abs(empty_base, kAbs);
  EXPECT_TRUE(resolve_abs.IsValid());
  EXPECT_EQ(kAbs, resolve_abs.GetString());

  // Resolving a non-relative URL agains the empty one should still error.
  const char kRel[] = "foo.html";
  KURL resolve_err(empty_base, kRel);
  EXPECT_FALSE(resolve_err.IsValid());
}

// WebKit will make empty URLs and set components on them. kurl doesn't allow
// replacements on invalid URLs, but here we do.
TEST(KURLTest, ReplaceInvalid) {
  KURL kurl;

  EXPECT_FALSE(kurl.IsValid());
  EXPECT_TRUE(kurl.IsEmpty());
  EXPECT_EQ("", kurl.GetString().Utf8());

  kurl.SetProtocol("http");
  // GKURL will say that a URL with just a scheme is invalid, KURL will not.
  EXPECT_FALSE(kurl.IsValid());
  EXPECT_FALSE(kurl.IsEmpty());
  // At this point, we do things slightly differently if there is only a scheme.
  // We check the results here to make it more obvious what is going on, but it
  // shouldn't be a big deal if these change.
  EXPECT_EQ("http:", kurl.GetString());

  kurl.SetHost("www.google.com");
  EXPECT_TRUE(kurl.IsValid());
  EXPECT_FALSE(kurl.IsEmpty());
  EXPECT_EQ("http://www.google.com/", kurl.GetString());

  kurl.SetPort(8000);
  EXPECT_TRUE(kurl.IsValid());
  EXPECT_FALSE(kurl.IsEmpty());
  EXPECT_EQ("http://www.google.com:8000/", kurl.GetString());

  kurl.SetPath("/favicon.ico");
  EXPECT_TRUE(kurl.IsValid());
  EXPECT_FALSE(kurl.IsEmpty());
  EXPECT_EQ("http://www.google.com:8000/favicon.ico", kurl.GetString());

  // Now let's test that giving an invalid replacement fails. Invalid
  // protocols fail without modifying the URL, which should remain valid.
  EXPECT_FALSE(kurl.SetProtocol("f/sj#@"));
  EXPECT_TRUE(kurl.IsValid());
}

TEST(KURLTest, Valid_HTTP_FTP_URLsHaveHosts) {
  KURL kurl("foo://www.google.com/");
  EXPECT_TRUE(kurl.SetProtocol("http"));
  EXPECT_TRUE(kurl.ProtocolIs("http"));
  EXPECT_TRUE(kurl.ProtocolIsInHTTPFamily());
  EXPECT_TRUE(kurl.IsValid());

  EXPECT_TRUE(kurl.SetProtocol("https"));
  EXPECT_TRUE(kurl.ProtocolIs("https"));
  EXPECT_TRUE(kurl.IsValid());

  EXPECT_TRUE(kurl.SetProtocol("ftp"));
  EXPECT_TRUE(kurl.ProtocolIs("ftp"));
  EXPECT_TRUE(kurl.IsValid());

  kurl = KURL("http://");
  EXPECT_FALSE(kurl.ProtocolIs("http"));

  kurl = KURL("http://wide#鸡");
  EXPECT_TRUE(kurl.ProtocolIs("http"));
  EXPECT_EQ(kurl.Protocol(), "http");

  kurl = KURL("https://foo");
  EXPECT_TRUE(kurl.ProtocolIs("https"));

  kurl = KURL("ftp://foo");
  EXPECT_TRUE(kurl.ProtocolIs("ftp"));

  kurl = KURL("http://host/");
  EXPECT_TRUE(kurl.IsValid());
  kurl.SetHost("");
  EXPECT_FALSE(kurl.IsValid());

  kurl = KURL("https://host/");
  EXPECT_TRUE(kurl.IsValid());
  kurl.SetHost("");
  EXPECT_FALSE(kurl.IsValid());

  kurl = KURL("ftp://host/");
  EXPECT_TRUE(kurl.IsValid());
  kurl.SetHost("");
  EXPECT_FALSE(kurl.IsValid());

  kurl = KURL("http:///noodles/pho.php");
  EXPECT_EQ("http://noodles/pho.php", kurl.GetString());
  EXPECT_EQ("noodles", kurl.Host());
  EXPECT_TRUE(kurl.IsValid());

  kurl = KURL("https://username:password@/");
  EXPECT_FALSE(kurl.IsValid());

  kurl = KURL("https://username:password@host/");
  EXPECT_TRUE(kurl.IsValid());
}

TEST(KURLTest, Path) {
  const char kInitial[] = "http://www.google.com/path/foo";
  KURL kurl(kInitial);

  // Clear by setting a null string.
  String null_string;
  EXPECT_TRUE(null_string.IsNull());
  kurl.SetPath(null_string);
  EXPECT_EQ("http://www.google.com/", kurl.GetString());
}

// Test that setting the query to different things works. Thq query is handled
// a littler differently than some of the other components.
TEST(KURLTest, Query) {
  const char kInitial[] = "http://www.google.com/search?q=awesome";
  KURL kurl(kInitial);

  // Clear by setting a null string.
  String null_string;
  EXPECT_TRUE(null_string.IsNull());
  kurl.SetQuery(null_string);
  EXPECT_EQ("http://www.google.com/search", kurl.GetString());

  // Clear by setting an empty string.
  kurl = KURL(kInitial);
  String empty_string("");
  EXPECT_FALSE(empty_string.IsNull());
  kurl.SetQuery(empty_string);
  EXPECT_EQ("http://www.google.com/search?", kurl.GetString());

  // Set with something that begins in a question mark.
  const char kQuestion[] = "?foo=bar";
  kurl.SetQuery(kQuestion);
  EXPECT_EQ("http://www.google.com/search?foo=bar", kurl.GetString());

  // Set with something that doesn't begin in a question mark.
  const char kQuery[] = "foo=bar";
  kurl.SetQuery(kQuery);
  EXPECT_EQ("http://www.google.com/search?foo=bar", kurl.GetString());
}

TEST(KURLTest, Ref) {
  const KURL kurl("http://foo/bar#baz");

  // Basic ref setting.
  KURL cur("http://foo/bar");
  cur.SetFragmentIdentifier("asdf");
  EXPECT_EQ("http://foo/bar#asdf", cur.GetString());
  cur = kurl;
  cur.SetFragmentIdentifier("asdf");
  EXPECT_EQ("http://foo/bar#asdf", cur.GetString());

  // Setting a ref to the empty string will set it to "#".
  cur = KURL("http://foo/bar");
  cur.SetFragmentIdentifier("");
  EXPECT_EQ("http://foo/bar#", cur.GetString());
  cur = kurl;
  cur.SetFragmentIdentifier("");
  EXPECT_EQ("http://foo/bar#", cur.GetString());

  // Setting the ref to the null string will clear it altogether.
  cur = KURL("http://foo/bar");
  cur.SetFragmentIdentifier(String());
  EXPECT_EQ("http://foo/bar", cur.GetString());
  cur = kurl;
  cur.SetFragmentIdentifier(String());
  EXPECT_EQ("http://foo/bar", cur.GetString());
}

TEST(KURLTest, Empty) {
  const KURL kurl;

  // First test that regular empty URLs are the same.
  EXPECT_TRUE(kurl.IsEmpty());
  EXPECT_FALSE(kurl.IsValid());
  EXPECT_TRUE(kurl.IsNull());
  EXPECT_TRUE(kurl.GetString().IsNull());
  EXPECT_TRUE(kurl.GetString().IsEmpty());

  // Test resolving a null URL on an empty string.
  const KURL kurl2(kurl, "");
  EXPECT_FALSE(kurl2.IsNull());
  EXPECT_TRUE(kurl2.IsEmpty());
  EXPECT_FALSE(kurl2.IsValid());
  EXPECT_FALSE(kurl2.GetString().IsNull());
  EXPECT_TRUE(kurl2.GetString().IsEmpty());
  EXPECT_FALSE(kurl2.GetString().IsNull());
  EXPECT_TRUE(kurl2.GetString().IsEmpty());

  // Resolve the null URL on a null string.
  const KURL kurl22(kurl, String());
  EXPECT_FALSE(kurl22.IsNull());
  EXPECT_TRUE(kurl22.IsEmpty());
  EXPECT_FALSE(kurl22.IsValid());
  EXPECT_FALSE(kurl22.GetString().IsNull());
  EXPECT_TRUE(kurl22.GetString().IsEmpty());
  EXPECT_FALSE(kurl22.GetString().IsNull());
  EXPECT_TRUE(kurl22.GetString().IsEmpty());

  // Test non-hierarchical schemes resolving. The actual URLs will be different.
  // WebKit's one will set the string to "something.gif" and we'll set it to an
  // empty string. I think either is OK, so we just check our behavior.
  const KURL kurl3(KURL("data:foo"), "something.gif");
  EXPECT_TRUE(kurl3.IsEmpty());
  EXPECT_FALSE(kurl3.IsValid());

  // Test for weird isNull string input,
  // see: http://bugs.webkit.org/show_bug.cgi?id=16487
  const KURL kurl4(kurl.GetString());
  EXPECT_TRUE(kurl4.IsEmpty());
  EXPECT_FALSE(kurl4.IsValid());
  EXPECT_TRUE(kurl4.GetString().IsNull());
  EXPECT_TRUE(kurl4.GetString().IsEmpty());

  // Resolving an empty URL on an invalid string.
  const KURL kurl5("foo.js");
  // We'll be empty in this case, but KURL won't be. Should be OK.
  // EXPECT_EQ(kurl5.isEmpty(), kurl5.isEmpty());
  // EXPECT_EQ(kurl5.getString().isEmpty(), kurl5.getString().isEmpty());
  EXPECT_FALSE(kurl5.IsValid());
  EXPECT_FALSE(kurl5.GetString().IsNull());

  // Empty string as input
  const KURL kurl6("");
  EXPECT_TRUE(kurl6.IsEmpty());
  EXPECT_FALSE(kurl6.IsValid());
  EXPECT_FALSE(kurl6.GetString().IsNull());
  EXPECT_TRUE(kurl6.GetString().IsEmpty());

  // Non-empty but invalid C string as input.
  const KURL kurl7("foo.js");
  // WebKit will actually say this URL has the string "foo.js" but is invalid.
  // We don't do that.
  // EXPECT_EQ(kurl7.isEmpty(), kurl7.isEmpty());
  EXPECT_FALSE(kurl7.IsValid());
  EXPECT_FALSE(kurl7.GetString().IsNull());
}

TEST(KURLTest, UserPass) {
  const char* src = "http://user:pass@google.com/";
  KURL kurl(src);

  // Clear just the username.
  kurl.SetUser("");
  EXPECT_EQ("http://:pass@google.com/", kurl.GetString());

  // Clear just the password.
  kurl = KURL(src);
  kurl.SetPass("");
  EXPECT_EQ("http://user@google.com/", kurl.GetString());

  // Now clear both.
  kurl.SetUser("");
  EXPECT_EQ("http://google.com/", kurl.GetString());
}

TEST(KURLTest, Offsets) {
  const char* src1 = "http://user:pass@google.com/foo/bar.html?baz=query#ref";
  const KURL kurl1(src1);

  EXPECT_EQ(17u, kurl1.HostStart());
  EXPECT_EQ(27u, kurl1.HostEnd());
  EXPECT_EQ(27u, kurl1.PathStart());
  EXPECT_EQ(40u, kurl1.PathEnd());
  EXPECT_EQ(32u, kurl1.PathAfterLastSlash());

  const char* src2 = "http://google.com/foo/";
  const KURL kurl2(src2);

  EXPECT_EQ(7u, kurl2.HostStart());
  EXPECT_EQ(17u, kurl2.HostEnd());
  EXPECT_EQ(17u, kurl2.PathStart());
  EXPECT_EQ(22u, kurl2.PathEnd());
  EXPECT_EQ(22u, kurl2.PathAfterLastSlash());

  const char* src3 = "javascript:foobar";
  const KURL kurl3(src3);

  EXPECT_EQ(11u, kurl3.HostStart());
  EXPECT_EQ(11u, kurl3.HostEnd());
  EXPECT_EQ(11u, kurl3.PathStart());
  EXPECT_EQ(17u, kurl3.PathEnd());
  EXPECT_EQ(11u, kurl3.PathAfterLastSlash());
}

TEST(KURLTest, DeepCopy) {
  const char kUrl[] = "http://www.google.com/";
  const KURL src(kUrl);
  EXPECT_TRUE(src.GetString() ==
              kUrl);  // This really just initializes the cache.
  const KURL dest = src.Copy();
  EXPECT_TRUE(dest.GetString() ==
              kUrl);  // This really just initializes the cache.

  // The pointers should be different for both UTF-8 and UTF-16.
  EXPECT_NE(dest.GetString().Impl(), src.GetString().Impl());
}

TEST(KURLTest, DeepCopyInnerURL) {
  const char kUrl[] = "filesystem:http://www.google.com/temporary/test.txt";
  const char kInnerURL[] = "http://www.google.com/temporary";
  const KURL src(kUrl);
  EXPECT_TRUE(src.GetString() == kUrl);
  EXPECT_TRUE(src.InnerURL()->GetString() == kInnerURL);
  const KURL dest = src.Copy();
  EXPECT_TRUE(dest.GetString() == kUrl);
  EXPECT_TRUE(dest.InnerURL()->GetString() == kInnerURL);
}

TEST(KURLTest, LastPathComponent) {
  const KURL url1("http://host/path/to/file.txt");
  EXPECT_EQ("file.txt", url1.LastPathComponent());

  const KURL invalid_utf8("http://a@9%aa%:/path/to/file.txt");
  EXPECT_EQ(String(), invalid_utf8.LastPathComponent());
}

TEST(KURLTest, IsHierarchical) {
  // Note that it's debatable whether "filesystem" URLs are or hierarchical.
  // They're currently registered in the url lib as standard; but the parsed
  // url never has a valid hostname (the inner URL does)."
  const char* standard_urls[] = {
      "http://host/path/to/file.txt",
      "ftp://andrew.cmu.edu/foo",
      "file:///path/to/resource",
      "file://hostname/etc/"
      "filesystem:http://www.google.com/type/",
      "filesystem:http://user:pass@google.com:21/blah#baz",
  };

  for (const char* input : standard_urls) {
    SCOPED_TRACE(input);
    KURL url(input);
    EXPECT_TRUE(url.IsHierarchical());
    EXPECT_TRUE(url.CanSetHostOrPort());
    EXPECT_TRUE(url.CanSetPathname());
  }

  const char* nonstandard_urls[] = {
      "blob:null/guid-goes-here",
      "blob:http://example.com/guid-goes-here",
      "http://a@9%aa%:/path/to/file.txt",
      "about:blank://hostname",
      "about:blank",
      "javascript:void(0);",
      "data:text/html,greetings",
  };

  for (const char* input : nonstandard_urls) {
    SCOPED_TRACE(input);
    KURL url(input);
    EXPECT_FALSE(url.IsHierarchical());
    EXPECT_FALSE(url.CanSetHostOrPort());
    EXPECT_FALSE(url.CanSetPathname());
  }
}

TEST(KURLTest, PathAfterLastSlash) {
  KURL url1("http://host/path/to/file.txt");
  EXPECT_EQ(20u, url1.PathAfterLastSlash());

  KURL invalid_utf8("http://a@9%aa%:/path/to/file.txt");
  EXPECT_EQ(0u, invalid_utf8.PathAfterLastSlash());
}

TEST(KURLTest, ProtocolIsInHTTPFamily) {
  const KURL url1("http://host/path/to/file.txt");
  EXPECT_TRUE(url1.ProtocolIsInHTTPFamily());

  const KURL invalid_utf8("http://a@9%aa%:/path/to/file.txt");
  EXPECT_FALSE(invalid_utf8.ProtocolIsInHTTPFamily());
}

TEST(KURLTest, ProtocolIs) {
  const KURL url1("foo://bar");
  EXPECT_TRUE(url1.ProtocolIs("foo"));
  EXPECT_FALSE(url1.ProtocolIs("foo-bar"));

  const KURL url2("foo-bar:");
  EXPECT_TRUE(url2.ProtocolIs("foo-bar"));
  EXPECT_FALSE(url2.ProtocolIs("foo"));

  const KURL invalid_utf8("http://a@9%aa%:");
  EXPECT_FALSE(invalid_utf8.ProtocolIs("http"));

  const KURL capital("HTTP://www.example.text");
  EXPECT_TRUE(capital.ProtocolIs("http"));
  EXPECT_EQ(capital.Protocol(), "http");
}

TEST(KURLTest, strippedForUseAsReferrer) {
  struct ReferrerCase {
    const char* input;
    const char* output;
  } referrer_cases[] = {
      {"data:text/html;charset=utf-8,<html></html>", ""},
      {"javascript:void(0);", ""},
      {"about:config", ""},
      {"https://www.google.com/", "https://www.google.com/"},
      {"http://me@news.google.com:8888/", "http://news.google.com:8888/"},
      {"http://:pass@news.google.com:8888/foo",
       "http://news.google.com:8888/foo"},
      {"http://me:pass@news.google.com:8888/", "http://news.google.com:8888/"},
      {"https://www.google.com/a?f#b", "https://www.google.com/a?f"},
      {"file:///tmp/test.html", ""},
      {"https://www.google.com/#", "https://www.google.com/"},
  };

  for (size_t i = 0; i < base::size(referrer_cases); i++) {
    const KURL kurl(referrer_cases[i].input);
    EXPECT_EQ(referrer_cases[i].output, kurl.StrippedForUseAsReferrer().Utf8());
  }
}

enum class PortIsValid {
  // The constructor does strict checking. Ports which are considered valid by
  // the constructor are kAlways valid.
  kAlways,

  // SetHostAndPort() truncates to the initial numerical prefix, and then does
  // strict checking. kInSetHostAndPort is used for ports which are considered
  // valid by SetHostAndPort() but not by the constructor. In this case, the
  // expected value is the same as for SetPort().
  kInSetHostAndPort,

  // SetPort() considers all input valid.
  kInSetPort
};

struct PortTestCase {
  const char* input;
  const uint16_t constructor_output;
  const uint16_t set_port_output;
  const PortIsValid is_valid;
};

// The tested behaviour matches the implementation. It doesn't necessarily match
// the URL Standard.
const PortTestCase port_test_cases[] = {
    {"80", 0, 0, PortIsValid::kAlways},  // 0 because scheme is http.
    {"443", 443, 443, PortIsValid::kAlways},
    {"8000", 8000, 8000, PortIsValid::kAlways},
    {"0", 0, 0, PortIsValid::kAlways},
    {"1", 1, 1, PortIsValid::kAlways},
    {"00000000000000000000000000000000000443", 443, 443, PortIsValid::kAlways},
    {"+80", 0, 0, PortIsValid::kInSetHostAndPort},
    {"-80", 0, 0, PortIsValid::kInSetHostAndPort},
    {"443e0", 0, 443, PortIsValid::kInSetHostAndPort},
    {"0x80", 0, 0, PortIsValid::kInSetHostAndPort},
    {"8%30", 0, 8, PortIsValid::kInSetHostAndPort},
    {" 443", 0, 0, PortIsValid::kInSetHostAndPort},
    {"443 ", 0, 443, PortIsValid::kInSetHostAndPort},
    {":443", 0, 0, PortIsValid::kInSetHostAndPort},
    {"65535", 65535, 65535, PortIsValid::kAlways},
    {"65534", 65534, 65534, PortIsValid::kAlways},
    {"65536", 0, 0, PortIsValid::kInSetPort},
    {"65537", 0, 1, PortIsValid::kInSetPort},
    {"2147483647", 0, 65535, PortIsValid::kInSetPort},
    {"2147483648", 0, 0, PortIsValid::kInSetPort},
    {"2147483649", 0, 1, PortIsValid::kInSetPort},
    {"4294967295", 0, 65535, PortIsValid::kInSetPort},
    {"4294967296", 0, 0, PortIsValid::kInSetPort},
    {"4294967297", 0, 0, PortIsValid::kInSetPort},
    {"18446744073709551615", 0, 0, PortIsValid::kInSetPort},
    {"18446744073709551616", 0, 0, PortIsValid::kInSetPort},
    {"18446744073709551617", 0, 0, PortIsValid::kInSetPort},
    {"9999999999999999999999999999990999999999", 0, 0, PortIsValid::kInSetPort},
};

void PrintTo(const PortTestCase& port_test_case, ::std::ostream* os) {
  *os << '"' << port_test_case.input << '"';
}

class KURLPortTest : public ::testing::TestWithParam<PortTestCase> {};

TEST_P(KURLPortTest, Construct) {
  const auto& param = GetParam();
  const KURL url(String("http://a:") + param.input + "/");
  EXPECT_EQ(url.Port(), param.constructor_output);
  if (param.is_valid == PortIsValid::kAlways) {
    EXPECT_EQ(url.IsValid(), true);
  } else {
    EXPECT_EQ(url.IsValid(), false);
  }
}

TEST_P(KURLPortTest, ConstructRelative) {
  const auto& param = GetParam();
  const KURL base("http://a/");
  const KURL url(base, String("//a:") + param.input + "/");
  EXPECT_EQ(url.Port(), param.constructor_output);
  if (param.is_valid == PortIsValid::kAlways) {
    EXPECT_EQ(url.IsValid(), true);
  } else {
    EXPECT_EQ(url.IsValid(), false);
  }
}

TEST_P(KURLPortTest, SetPort) {
  const auto& param = GetParam();
  KURL url("http://a:8888/");
  url.SetPort(param.input);
  EXPECT_EQ(url.Port(), param.set_port_output);
  EXPECT_EQ(url.IsValid(), true);
}

TEST_P(KURLPortTest, SetHostAndPort) {
  const auto& param = GetParam();
  KURL url("http://a:8888/");
  url.SetHostAndPort(String("a:") + param.input);
  switch (param.is_valid) {
    case PortIsValid::kAlways:
      EXPECT_EQ(url.Port(), param.constructor_output);
      EXPECT_EQ(url.IsValid(), true);
      break;

    case PortIsValid::kInSetHostAndPort:
      EXPECT_EQ(url.Port(), param.set_port_output);
      EXPECT_EQ(url.IsValid(), true);
      break;

    case PortIsValid::kInSetPort:
      EXPECT_EQ(url.Port(), param.constructor_output);
      EXPECT_EQ(url.IsValid(), false);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(, KURLPortTest, ::testing::ValuesIn(port_test_cases));

}  // namespace blink
