// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_util.h"

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(MimeUtilTest, ExtensionTest) {
  // String: png\0css
  base::FilePath::StringType containsNullByte;
  containsNullByte.append(FILE_PATH_LITERAL("png"));
  containsNullByte.append(1, FILE_PATH_LITERAL('\0'));
  containsNullByte.append(FILE_PATH_LITERAL("css"));

  const struct {
    const base::FilePath::StringType extension;
    const char* const mime_type;
    bool valid;
  } tests[] = {
    {FILE_PATH_LITERAL("png"), "image/png", true},
    {FILE_PATH_LITERAL("PNG"), "image/png", true},
    {FILE_PATH_LITERAL("css"), "text/css", true},
    {FILE_PATH_LITERAL("pjp"), "image/jpeg", true},
    {FILE_PATH_LITERAL("pjpeg"), "image/jpeg", true},
    {FILE_PATH_LITERAL("json"), "application/json", true},
    {FILE_PATH_LITERAL("js"), "text/javascript", true},
    {FILE_PATH_LITERAL("webm"), "video/webm", true},
    {FILE_PATH_LITERAL("weba"), "audio/webm", true},
#if defined(OS_CHROMEOS)
    // These are test cases for testing platform mime types on Chrome OS.
    {FILE_PATH_LITERAL("epub"), "application/epub+zip", true},
    {FILE_PATH_LITERAL("apk"), "application/vnd.android.package-archive", true},
    {FILE_PATH_LITERAL("cer"), "application/x-x509-ca-cert", true},
    {FILE_PATH_LITERAL("crt"), "application/x-x509-ca-cert", true},
    {FILE_PATH_LITERAL("zip"), "application/zip", true},
    {FILE_PATH_LITERAL("ics"), "text/calendar", true},
#endif
#if defined(OS_ANDROID)
    {FILE_PATH_LITERAL("m3u8"), "application/x-mpegurl", true},
#endif
    {FILE_PATH_LITERAL("not an extension / for sure"), "", false},
    {containsNullByte, "", false}
  };

  std::string mime_type;
  bool rv;

  for (const auto& test : tests) {
    rv = GetMimeTypeFromExtension(test.extension, &mime_type);
    EXPECT_EQ(test.valid, rv);
    if (rv)
      EXPECT_EQ(test.mime_type, mime_type);
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

TEST(MimeUtilTest, TestParseMimeTypeWithoutParameter) {
  std::string nonAscii("application/nonutf8");
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(nonAscii, nullptr, nullptr));
#if defined(OS_WIN)
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

  //EXPECT_TRUE(ParseMimeTypeWithoutParameter("video/mime;parameter"));
}

TEST(MimeUtilTest, TestIsValidTopLevelMimeType) {
  EXPECT_TRUE(IsValidTopLevelMimeType("application"));
  EXPECT_TRUE(IsValidTopLevelMimeType("audio"));
  EXPECT_TRUE(IsValidTopLevelMimeType("example"));
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
      // Convert ASCII to FilePath::StringType.
      base::FilePath::StringType contained_result(
          test.contained_result,
          test.contained_result + strlen(test.contained_result));

      bool found = base::Contains(extensions, contained_result);

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
