// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_util.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using testing::Contains;

TEST(MimeUtilTest, GetWellKnownMimeTypeFromExtension) {
  // String: png\0css
  base::FilePath::StringType containsNullByte;
  containsNullByte.append(FILE_PATH_LITERAL("png"));
  containsNullByte.append(1, FILE_PATH_LITERAL('\0'));
  containsNullByte.append(FILE_PATH_LITERAL("css"));

  const struct {
    const base::FilePath::StringType extension;
    const char* const mime_type;
  } tests[] = {
      {FILE_PATH_LITERAL("png"), "image/png"},
      {FILE_PATH_LITERAL("PNG"), "image/png"},
      {FILE_PATH_LITERAL("css"), "text/css"},
      {FILE_PATH_LITERAL("pjp"), "image/jpeg"},
      {FILE_PATH_LITERAL("pjpeg"), "image/jpeg"},
      {FILE_PATH_LITERAL("json"), "application/json"},
      {FILE_PATH_LITERAL("js"), "text/javascript"},
      {FILE_PATH_LITERAL("webm"), "video/webm"},
      {FILE_PATH_LITERAL("weba"), "audio/webm"},
      {FILE_PATH_LITERAL("avif"), "image/avif"},
      {FILE_PATH_LITERAL("epub"), "application/epub+zip"},
      {FILE_PATH_LITERAL("apk"), "application/vnd.android.package-archive"},
      {FILE_PATH_LITERAL("cer"), "application/x-x509-ca-cert"},
      {FILE_PATH_LITERAL("crt"), "application/x-x509-ca-cert"},
      {FILE_PATH_LITERAL("zip"), "application/zip"},
      {FILE_PATH_LITERAL("ics"), "text/calendar"},
      {FILE_PATH_LITERAL("m3u8"), "application/x-mpegurl"},
      {FILE_PATH_LITERAL("csv"), "text/csv"},
      {FILE_PATH_LITERAL("not an extension / for sure"), nullptr},
      {containsNullByte, nullptr}};

  for (const auto& test : tests) {
    std::string mime_type;
    if (GetWellKnownMimeTypeFromExtension(test.extension, &mime_type))
      EXPECT_EQ(test.mime_type, mime_type);
    else
      EXPECT_EQ(test.mime_type, nullptr);
  }
}

TEST(MimeUtilTest, ExtensionTest) {
  // String: png\0css
  base::FilePath::StringType containsNullByte;
  containsNullByte.append(FILE_PATH_LITERAL("png"));
  containsNullByte.append(1, FILE_PATH_LITERAL('\0'));
  containsNullByte.append(FILE_PATH_LITERAL("css"));

  const struct {
    const base::FilePath::StringType extension;
    const std::vector<std::string> mime_types;
  } tests[] = {
    {FILE_PATH_LITERAL("png"), {"image/png"}},
    {FILE_PATH_LITERAL("PNG"), {"image/png"}},
    {FILE_PATH_LITERAL("css"), {"text/css"}},
    {FILE_PATH_LITERAL("pjp"), {"image/jpeg"}},
    {FILE_PATH_LITERAL("pjpeg"), {"image/jpeg"}},
    {FILE_PATH_LITERAL("json"), {"application/json"}},
    {FILE_PATH_LITERAL("js"), {"text/javascript"}},
    {FILE_PATH_LITERAL("webm"), {"video/webm"}},
    {FILE_PATH_LITERAL("weba"), {"audio/webm"}},
    {FILE_PATH_LITERAL("avif"), {"image/avif"}},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // These are test cases for testing platform mime types on ChromeOS.
    {FILE_PATH_LITERAL("epub"), {"application/epub+zip"}},
    {FILE_PATH_LITERAL("apk"), {"application/vnd.android.package-archive"}},
    {FILE_PATH_LITERAL("cer"),
     {
         "application/x-x509-ca-cert",
         "application/pkix-cert",  // System override for ChromeOS.
     }},
    {FILE_PATH_LITERAL("crt"),
     {
         "application/x-x509-ca-cert",
         "application/pkix-cert",  // System override for ChromeOS.
     }},
    {FILE_PATH_LITERAL("zip"), {"application/zip"}},
    {FILE_PATH_LITERAL("ics"), {"text/calendar"}},
#endif
    {FILE_PATH_LITERAL("m3u8"),
     {
         "application/x-mpegurl",  // Chrome's secondary mapping.
         "audio/x-mpegurl",  // https://crbug.com/1273061, system override for
                             // android-arm[64]-test and Linux. Possibly more.
         "audio/mpegurl",                  // System override for mac.
     }},
    {FILE_PATH_LITERAL("csv"), {"text/csv"}},
    {FILE_PATH_LITERAL("not an extension / for sure"), {}},
    {containsNullByte, {}}
  };

  for (const auto& test : tests) {
    std::string mime_type;
    if (GetMimeTypeFromExtension(test.extension, &mime_type))
      EXPECT_THAT(test.mime_types, Contains(mime_type));
    else
      EXPECT_TRUE(test.mime_types.empty());
  }
}

// Behavior of GetPreferredExtensionForMimeType() is dependent on the host
// platform since the latter can override the mapping from file extensions to
// MIME types. The tests below would only work if the platform MIME mappings
// don't have mappings for or has an agreeing mapping for each MIME type
// mentioned.
TEST(MimeUtilTest, GetPreferredExtensionForMimeType) {
  const struct {
    const std::string mime_type;
    const base::FilePath::StringType expected_extension;
  } kTestCases[] = {
      {"application/wasm", FILE_PATH_LITERAL("wasm")},      // Primary
      {"application/javascript", FILE_PATH_LITERAL("js")},  // Secondary
      {"text/javascript", FILE_PATH_LITERAL("js")},         // Primary
      {"video/webm", FILE_PATH_LITERAL("webm")},            // Primary
  };

  for (const auto& test : kTestCases) {
    base::FilePath::StringType extension;
    auto rv = GetPreferredExtensionForMimeType(test.mime_type, &extension);
    EXPECT_TRUE(rv);
    EXPECT_EQ(test.expected_extension, extension);
  }
}

TEST(MimeUtilTest, FileTest) {
  const struct {
    const base::FilePath::CharType* file_path;
    const char* const mime_type;
    bool valid;
  } tests[] = {
      {FILE_PATH_LITERAL("c:\\foo\\bar.css"), "text/css", true},
      {FILE_PATH_LITERAL("c:\\foo\\bar.CSS"), "text/css", true},
      {FILE_PATH_LITERAL("c:\\blah"), "", false},
      {FILE_PATH_LITERAL("/usr/local/bin/mplayer"), "", false},
      {FILE_PATH_LITERAL("/home/foo/bar.css"), "text/css", true},
      {FILE_PATH_LITERAL("/blah."), "", false},
      {FILE_PATH_LITERAL("c:\\blah."), "", false},
  };

  std::string mime_type;
  bool rv;

  for (const auto& test : tests) {
    rv = GetMimeTypeFromFile(base::FilePath(test.file_path), &mime_type);
    EXPECT_EQ(test.valid, rv);
    if (rv)
      EXPECT_EQ(test.mime_type, mime_type);
  }
}

TEST(MimeUtilTest, MatchesMimeType) {
  // MIME types are case insensitive.
  EXPECT_TRUE(MatchesMimeType("VIDEO/*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "VIDEO/X-MPEG"));

  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml",
                                   "application/html+xml"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml", "application/+xml"));
  EXPECT_TRUE(MatchesMimeType("application/*+json",
                                   "application/x-myformat+json"));
  EXPECT_TRUE(MatchesMimeType("aaa*aaa", "aaaaaa"));
  EXPECT_TRUE(MatchesMimeType("*", std::string()));
  EXPECT_FALSE(MatchesMimeType("video/", "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType("VIDEO/", "Video/X-MPEG"));
  EXPECT_FALSE(MatchesMimeType(std::string(), "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType(std::string(), std::string()));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg", std::string()));
  EXPECT_FALSE(MatchesMimeType("application/*+xml", "application/xml"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "application/html+xmlz"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "applcation/html+xml"));
  EXPECT_FALSE(MatchesMimeType("aaa*aaa", "aaaaa"));

  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("*", "Video/X-MPEG;PARAM=VAL"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg"));
  EXPECT_FALSE(MatchesMimeType("Video/*;PARAM=VAL", "VIDEO/Mpeg"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg;param=other"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("Video/*;PARAM=Val", "VIDEO/Mpeg;Param=Val"));
  EXPECT_FALSE(MatchesMimeType("Video/*;PARAM=VAL", "VIDEO/Mpeg;Param=Val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param2=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("Video/X-Mpeg;Param=Val",
                              "VIDEO/X-MPEG;PARAM=Val;PARAM2=val2"));
  EXPECT_TRUE(MatchesMimeType("Video/X-Mpeg;Param=VAL",
                              "VIDEO/X-MPEG;PARAM=VAL;PARAM2=val2"));
  EXPECT_FALSE(MatchesMimeType("Video/X-Mpeg;Param=val",
                               "VIDEO/X-MPEG;PARAM=VAL;PARAM2=val2"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param=VAL;param2=val2",
                               "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param2=val2;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param3=val3;param=val",
                               "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val ;param2=val2 ",
                              "video/x-mpeg;param=val;param2=val2"));

  EXPECT_TRUE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val2"));

  EXPECT_TRUE(MatchesMimeType("*", "*"));
  EXPECT_TRUE(MatchesMimeType("*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_FALSE(MatchesMimeType("video/*", "*/*"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/*;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*;param=val2"));

  EXPECT_TRUE(MatchesMimeType("ab*cd", "abxxxcd"));
  EXPECT_TRUE(MatchesMimeType("ab*cd", "abx/xcd"));
  EXPECT_TRUE(MatchesMimeType("ab/*cd", "ab/xxxcd"));
}

TEST(MimeUtilTest, TestParseMimeType) {
  const struct {
    std::string type_str;
    std::string mime_type;
    base::StringPairs params;
  } tests[] = {
      // Simple tests.
      {"image/jpeg", "image/jpeg"},
      {"application/octet-stream;foo=bar;name=\"test.jpg\"",
       "application/octet-stream",
       {{"foo", "bar"}, {"name", "test.jpg"}}},
      // Quoted string parsing.
      {"t/s;name=\"t\\\\est\\\".jpg\"", "t/s", {{"name", "t\\est\".jpg"}}},
      {"t/s;name=\"test.jpg\"", "t/s", {{"name", "test.jpg"}}},
      {"t/s;name=\"test;jpg\"", "t/s", {{"name", "test;jpg"}}},
      // Lenient for no closing quote.
      {"t/s;name=\"test.jpg", "t/s", {{"name", "test.jpg"}}},
      {"t/s;name=\"ab\\\"", "t/s", {{"name", "ab\""}}},
      // Strip whitespace from start/end of mime_type.
      {" t/s", "t/s"},
      {"t/s ", "t/s"},
      {" t/s ", "t/s"},
      {"t/=", "t/="},
      // Generally ignore whitespace.
      {"t/s;a=1;b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      {"t/s ;a=1;b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      {"t/s; a=1;b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      // Special case, include whitespace after param name until equals.
      {"t/s;a =1;b=2", "t/s", {{"a ", "1"}, {"b", "2"}}},
      {"t/s;a= 1;b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      {"t/s;a=1 ;b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      {"t/s;a=1; b=2", "t/s", {{"a", "1"}, {"b", "2"}}},
      {"t/s; a = 1;b=2", "t/s", {{"a ", "1"}, {"b", "2"}}},
      // Do not trim whitespace from quoted-string param values.
      {"t/s;a=\" 1\";b=2", "t/s", {{"a", " 1"}, {"b", "2"}}},
      {"t/s;a=\"1 \";b=2", "t/s", {{"a", "1 "}, {"b", "2"}}},
      {"t/s;a=\" 1 \";b=2", "t/s", {{"a", " 1 "}, {"b", "2"}}},
      // Ignore incomplete params.
      {"t/s;a", "t/s", {}},
      {"t/s;a=", "t/s", {}},
      {"t/s;a=1;", "t/s", {{"a", "1"}}},
      {"t/s;a=1;b", "t/s", {{"a", "1"}}},
      {"t/s;a=1;b=", "t/s", {{"a", "1"}}},
      // Allow empty subtype.
      {"t/", "t/", {}},
      {"ts/", "ts/", {}},
      {"t/;", "t/", {}},
      {"t/ s", "t/", {}},
      // Questionable: allow anything as long as there is a slash somewhere.
      {"/ts", "/ts", {}},
      {"/s", "/s", {}},
      {"/", "/", {}},
  };
  for (const auto& test : tests) {
    std::string mime_type;
    base::StringPairs params;
    EXPECT_TRUE(ParseMimeType(test.type_str, &mime_type, &params));
    EXPECT_EQ(test.mime_type, mime_type);
    EXPECT_EQ(test.params, params);
  }
  for (auto* type_str : {
           // Must have slash in mime type.
           "",
           "ts",
           "t / s",
       }) {
    EXPECT_FALSE(ParseMimeType(type_str, nullptr, nullptr));
  }
}

TEST(MimeUtilTest, TestParseMimeTypeWithoutParameter) {
  std::string nonAscii("application/nonutf8");
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(nonAscii, nullptr, nullptr));
#if BUILDFLAG(IS_WIN)
  nonAscii.append(base::WideToUTF8(L"\u2603"));
#else
  nonAscii.append("\u2603");  // unicode snowman
#endif
  EXPECT_FALSE(ParseMimeTypeWithoutParameter(nonAscii, nullptr, nullptr));

  std::string top_level_type;
  std::string subtype;
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(
      "application/mime", &top_level_type, &subtype));
  EXPECT_EQ("application", top_level_type);
  EXPECT_EQ("mime", subtype);

  // Various allowed subtype forms.
  EXPECT_TRUE(
      ParseMimeTypeWithoutParameter("application/json", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("application/x-suggestions+json",
                                            nullptr, nullptr));
  EXPECT_TRUE(
      ParseMimeTypeWithoutParameter("application/+json", nullptr, nullptr));

  // Upper case letters are allowed.
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/mime", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("TEXT/mime", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("Text/mime", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("TeXt/mime", nullptr, nullptr));

  // Experimental types are also considered to be valid.
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("x-video/mime", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("X-Video/mime", nullptr, nullptr));

  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/ ", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("te(xt/ ", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/()plain", nullptr, nullptr));

  EXPECT_FALSE(ParseMimeTypeWithoutParameter("x-video", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("x-video/", nullptr, nullptr));

  EXPECT_FALSE(
      ParseMimeTypeWithoutParameter("application/a/b/c", nullptr, nullptr));

  // Test leading and trailing whitespace
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(" text/plain", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/plain ", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text /plain", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/ plain ", nullptr, nullptr));

  EXPECT_TRUE(ParseMimeTypeWithoutParameter("\ttext/plain", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/plain\t", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text\t/plain", nullptr, nullptr));
  EXPECT_FALSE(
      ParseMimeTypeWithoutParameter("text/\tplain ", nullptr, nullptr));

  EXPECT_TRUE(ParseMimeTypeWithoutParameter("\vtext/plain", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/plain\v", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text\v/plain", nullptr, nullptr));
  EXPECT_FALSE(
      ParseMimeTypeWithoutParameter("text/\vplain ", nullptr, nullptr));

  EXPECT_TRUE(ParseMimeTypeWithoutParameter("\rtext/plain", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/plain\r", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text\r/plain", nullptr, nullptr));
  EXPECT_FALSE(
      ParseMimeTypeWithoutParameter("text/\rplain ", nullptr, nullptr));

  EXPECT_TRUE(ParseMimeTypeWithoutParameter("\ntext/plain", nullptr, nullptr));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/plain\n", nullptr, nullptr));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text\n/plain", nullptr, nullptr));
  EXPECT_FALSE(
      ParseMimeTypeWithoutParameter("text/\nplain ", nullptr, nullptr));
}

class ExtractMIMETypeTestInvalid : public testing::TestWithParam<std::string> {
};

INSTANTIATE_TEST_SUITE_P(
    InvalidMediaTypes,
    ExtractMIMETypeTestInvalid,
    testing::Values(
        // Fails because it doesn't contain '/'.
        "a",
        "application",
        // Space is not HTTP token code point.
        //  https://mimesniff.spec.whatwg.org/#http-token-code-point
        // U+2003, EM SPACE (UTF-8: E2 80 83).
        "\xE2\x80\x83text/html",
        "text\xE2\x80\x83/html",
        "text / html",
        "t e x t / h t m l",
        "text\r\n/\nhtml",
        "text\n/\nhtml",
        ", text/html",
        "; text/html"));

TEST_P(ExtractMIMETypeTestInvalid, MustFail) {
  // Parsing is expected to fail.
  EXPECT_EQ(std::nullopt, net::ExtractMimeTypeFromMediaType(GetParam(), true));
}

class ExtractMIMETypeTestValid : public testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    ValidMediaTypes,
    ExtractMIMETypeTestValid,
    testing::Values("text/html",
                    "text/html; charset=iso-8859-1",
                    // Quoted charset parameter.
                    "text/html; charset=\"quoted\"",
                    // Multiple parameters.
                    "text/html; charset=x; foo=bar",
                    // OWSes are trimmed.
                    " text/html   ",
                    "\ttext/html \t",
                    "text/html ; charset=iso-8859-1"
                    // Non-standard multiple type/subtype listing using a comma
                    // as a separator is accepted.
                    "text/html,text/plain",
                    "text/html , text/plain",
                    "text/html\t,\ttext/plain",
                    "text/html,text/plain;charset=iso-8859-1",
                    "\r\ntext/html\r\n",
                    "text/html;wow",
                    "text/html;;;;;;",
                    "text/html; = = = "));

TEST_P(ExtractMIMETypeTestValid, MustSucceed) {
  //  net::ExtractMIMETypeFromMediaType parses well-formed headers correctly.
  EXPECT_EQ("text/html",
            net::ExtractMimeTypeFromMediaType(GetParam(), true).value_or(""));
}

TEST(MimeUtilTest, TestIsValidTopLevelMimeType) {
  EXPECT_TRUE(IsValidTopLevelMimeType("application"));
  EXPECT_TRUE(IsValidTopLevelMimeType("audio"));
  EXPECT_TRUE(IsValidTopLevelMimeType("example"));
  EXPECT_TRUE(IsValidTopLevelMimeType("font"));
  EXPECT_TRUE(IsValidTopLevelMimeType("image"));
  EXPECT_TRUE(IsValidTopLevelMimeType("message"));
  EXPECT_TRUE(IsValidTopLevelMimeType("model"));
  EXPECT_TRUE(IsValidTopLevelMimeType("multipart"));
  EXPECT_TRUE(IsValidTopLevelMimeType("text"));
  EXPECT_TRUE(IsValidTopLevelMimeType("video"));

  EXPECT_TRUE(IsValidTopLevelMimeType("TEXT"));
  EXPECT_TRUE(IsValidTopLevelMimeType("Text"));
  EXPECT_TRUE(IsValidTopLevelMimeType("TeXt"));

  EXPECT_FALSE(IsValidTopLevelMimeType("mime"));
  EXPECT_FALSE(IsValidTopLevelMimeType(""));
  EXPECT_FALSE(IsValidTopLevelMimeType("/"));
  EXPECT_FALSE(IsValidTopLevelMimeType(" "));

  EXPECT_TRUE(IsValidTopLevelMimeType("x-video"));
  EXPECT_TRUE(IsValidTopLevelMimeType("X-video"));

  EXPECT_FALSE(IsValidTopLevelMimeType("x-"));
}

TEST(MimeUtilTest, TestGetExtensionsForMimeType) {
  const struct {
    const char* const mime_type;
    size_t min_expected_size;
    const char* const contained_result;
    bool no_matches;
  } tests[] = {
      {"text/plain", 2, "txt"},
      {"text/pl", 0, nullptr, true},
      {"*", 0, nullptr},
      {"", 0, nullptr, true},
      {"message/*", 1, "eml"},
      {"MeSsAge/*", 1, "eml"},
      {"message/", 0, nullptr, true},
      {"image/avif", 1, "avif"},
      {"image/bmp", 1, "bmp"},
      {"video/*", 6, "mp4"},
      {"video/*", 6, "mpeg"},
      {"audio/*", 6, "oga"},
      {"aUDIo/*", 6, "wav"},
  };

  for (const auto& test : tests) {
    std::vector<base::FilePath::StringType> extensions;
    GetExtensionsForMimeType(test.mime_type, &extensions);
    ASSERT_LE(test.min_expected_size, extensions.size());

    if (test.no_matches)
      ASSERT_EQ(0u, extensions.size());

    if (test.contained_result) {
      bool found = base::Contains(
          extensions, base::FilePath::FromASCII(test.contained_result).value());

      ASSERT_TRUE(found) << "Must find at least the contained result within "
                         << test.mime_type;
    }
  }
}

TEST(MimeUtilTest, TestGenerateMimeMultipartBoundary) {
  std::string boundary1 = GenerateMimeMultipartBoundary();
  std::string boundary2 = GenerateMimeMultipartBoundary();

  // RFC 1341 says: the boundary parameter [...] consists of 1 to 70 characters.
  EXPECT_GE(70u, boundary1.size());
  EXPECT_GE(70u, boundary2.size());

  // RFC 1341 asks to: exercise care to choose a unique boundary.
  EXPECT_NE(boundary1, boundary2);
  ASSERT_LE(16u, boundary1.size());
  ASSERT_LE(16u, boundary2.size());

  // Expect that we don't pick '\0' character from the array/string
  // where we take the characters from.
  EXPECT_EQ(std::string::npos, boundary1.find('\0'));
  EXPECT_EQ(std::string::npos, boundary2.find('\0'));

  // Asserts below are not RFC 1341 requirements, but are here
  // to improve readability of generated MIME documents and to
  // try to preserve some aspects of the old boundary generation code.
  EXPECT_EQ("--", boundary1.substr(0, 2));
  EXPECT_EQ("--", boundary2.substr(0, 2));
  EXPECT_NE(std::string::npos, boundary1.find("MultipartBoundary"));
  EXPECT_NE(std::string::npos, boundary2.find("MultipartBoundary"));
  EXPECT_EQ("--", boundary1.substr(boundary1.size() - 2, 2));
  EXPECT_EQ("--", boundary2.substr(boundary2.size() - 2, 2));
}

TEST(MimeUtilTest, TestAddMultipartValueForUpload) {
  const char ref_output[] =
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"\r\nContent-Type: content type"
      "\r\n\r\nvalue\r\n"
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"\r\n\r\nvalue\r\n"
      "--boundary--\r\n";
  std::string post_data;
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "content type", &post_data);
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "", &post_data);
  AddMultipartFinalDelimiterForUpload("boundary", &post_data);
  EXPECT_STREQ(ref_output, post_data.c_str());
}

TEST(MimeUtilTest, TestAddMultipartValueForUploadWithFileName) {
  const char ref_output[] =
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"; filename=\"file name\"\r\nContent-Type: content "
      "type"
      "\r\n\r\nvalue\r\n"
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"; filename=\"file name\"\r\n\r\nvalue\r\n"
      "--boundary--\r\n";
  std::string post_data;
  AddMultipartValueForUploadWithFileName("value name", "file name", "value",
                                         "boundary", "content type",
                                         &post_data);
  AddMultipartValueForUploadWithFileName("value name", "file name", "value",
                                         "boundary", "", &post_data);
  AddMultipartFinalDelimiterForUpload("boundary", &post_data);
  EXPECT_STREQ(ref_output, post_data.c_str());
}
}  // namespace net
