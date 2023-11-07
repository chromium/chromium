// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_sniffer.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace net {
namespace {

using ::testing::Range;
using ::testing::Values;
using ::net::SniffMimeType;  // It is shadowed by SniffMimeType(), below.

// Turn |str|, a constant string with one or more embedded NULs, along with
// a NUL terminator, into an std::string() containing just that data.
// Turn |str|, a string with one or more embedded NULs, into an std::string()
template <size_t N>
std::string MakeConstantString(const char (&str)[N]) {
  return std::string(str, N - 1);
}

static std::string SniffMimeType(std::string_view content,
                                 const std::string& url,
                                 const std::string& mime_type_hint) {
  std::string mime_type;
  SniffMimeType(content, GURL(url), mime_type_hint,
                ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  return mime_type;
}

TEST(MimeSnifferTest, SniffableSchemes) {
  struct {
    const char* scheme;
    bool sniffable;
  } kTestCases[] = {
    {url::kAboutScheme, false},
    {url::kBlobScheme, false},
#if BUILDFLAG(IS_ANDROID)
    {url::kContentScheme, true},
#else
    {url::kContentScheme, false},
#endif
    {url::kContentIDScheme, false},
    {url::kDataScheme, false},
    {url::kFileScheme, true},
    {url::kFileSystemScheme, true},
    {url::kFtpScheme, false},
    {url::kHttpScheme, true},
    {url::kHttpsScheme, true},
    {url::kJavaScriptScheme, false},
    {url::kMailToScheme, false},
    {url::kWsScheme, false},
    {url::kWssScheme, false}
  };

  for (const auto test_case : kTestCases) {
    GURL url(std::string(test_case.scheme) + "://host/path/whatever");
    EXPECT_EQ(test_case.sniffable, ShouldSniffMimeType(url, ""));
  }
}

TEST(MimeSnifferTest, BoundaryConditionsTest) {
  std::string mime_type;
  std::string type_hint;

  char buf[] = {
    'd', '\x1f', '\xFF'
  };

  GURL url;

  SniffMimeType(std::string_view(), url, type_hint,
                ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  EXPECT_EQ("text/plain", mime_type);
  SniffMimeType(std::string_view(buf, 1), url, type_hint,
                ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  EXPECT_EQ("text/plain", mime_type);
  SniffMimeType(std::string_view(buf, 2), url, type_hint,
                ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  EXPECT_EQ("application/octet-stream", mime_type);
}

TEST(MimeSnifferTest, BasicSniffingTest) {
  EXPECT_EQ("text/html",
            SniffMimeType(MakeConstantString("<!DOCTYPE html PUBLIC"),
                          "http://www.example.com/", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("<HtMl><Body></body></htMl>"),
                          "http://www.example.com/foo.gif",
                          "application/octet-stream"));
  EXPECT_EQ("image/gif",
            SniffMimeType(MakeConstantString("GIF89a\x1F\x83\x94"),
                          "http://www.example.com/foo", "text/plain"));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Gif87a\x1F\x83\x94"),
                          "http://www.example.com/foo?param=tt.gif", ""));
  EXPECT_EQ("text/plain",
            SniffMimeType(MakeConstantString("%!PS-Adobe-3.0"),
                          "http://www.example.com/foo", "text/plain"));
  EXPECT_EQ(
      "application/octet-stream",
      SniffMimeType(MakeConstantString("\x89"
                                       "PNG\x0D\x0A\x1A\x0A"),
                    "http://www.example.com/foo", "application/octet-stream"));
  EXPECT_EQ("image/jpeg",
            SniffMimeType(MakeConstantString("\xFF\xD8\xFF\x23\x49\xAF"),
                          "http://www.example.com/foo", ""));
}

TEST(MimeSnifferTest, ChromeExtensionsTest) {
  // schemes
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx", ""));
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "https://www.example.com/foo.crx", ""));
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "ftp://www.example.com/foo.crx", ""));

  // some other mimetypes that should get converted
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx", "text/plain"));
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx",
                          "application/octet-stream"));

  // success edge cases
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx?query=string", ""));
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo..crx", ""));
  EXPECT_EQ("application/x-chrome-extension",
            SniffMimeType(MakeConstantString("Cr24\x03\x00\x00\x00"),
                          "http://www.example.com/foo..crx", ""));

  // wrong file extension
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.bin", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.bin?monkey", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "invalid-url", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foocrx", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx.blech", ""));

  // wrong magic
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("Cr24\x02\x00\x00\x01"),
                          "http://www.example.com/foo.crx?monkey", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("PADDING_Cr24\x02\x00\x00\x00"),
                          "http://www.example.com/foo.crx?monkey", ""));
}

TEST(MimeSnifferTest, MozillaCompatibleTest) {
  EXPECT_EQ("text/html", SniffMimeType(MakeConstantString(" \n <hTmL>\n <hea"),
                                       "http://www.example.com/", ""));
  EXPECT_EQ("text/plain",
            SniffMimeType(MakeConstantString(" \n <hTmL>\n <hea"),
                          "http://www.example.com/", "text/plain"));
  EXPECT_EQ("image/bmp", SniffMimeType(MakeConstantString("BMjlakdsfk"),
                                       "http://www.example.com/foo", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("\x00\x00\x30\x00"),
                          "http://www.example.com/favicon.ico", ""));
  EXPECT_EQ("text/plain", SniffMimeType(MakeConstantString("#!/bin/sh\nls /\n"),
                                        "http://www.example.com/foo", ""));
  EXPECT_EQ("text/plain",
            SniffMimeType(MakeConstantString("From: Fred\nTo: Bob\n\nHi\n.\n"),
                          "http://www.example.com/foo", ""));
  EXPECT_EQ("text/xml",
            SniffMimeType(MakeConstantString(
                              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"),
                          "http://www.example.com/foo", ""));
  EXPECT_EQ(
      "application/octet-stream",
      SniffMimeType(
          MakeConstantString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"),
          "http://www.example.com/foo", "application/octet-stream"));
}

TEST(MimeSnifferTest, DontAllowPrivilegeEscalationTest) {
  EXPECT_EQ(
      "image/gif",
      SniffMimeType(MakeConstantString("GIF87a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo", ""));
  EXPECT_EQ(
      "image/gif",
      SniffMimeType(MakeConstantString("GIF87a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo?q=ttt.html", ""));
  EXPECT_EQ(
      "image/gif",
      SniffMimeType(MakeConstantString("GIF87a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo#ttt.html", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString("a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString("a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo?q=ttt.html", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString("a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo#ttt.html", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString("a\n<html>\n<body>"
                                       "<script>alert('haxorzed');\n</script>"
                                       "</body></html>\n"),
                    "http://www.example.com/foo.html", ""));
}

TEST(MimeSnifferTest, SniffFilesAsHtml) {
  const std::string kContent = "<html><body>text</body></html>";
  const GURL kUrl("file:///C/test.unusualextension");

  std::string mime_type;
  SniffMimeType(kContent, kUrl, "" /* type_hint */,
                ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  EXPECT_EQ("text/plain", mime_type);

  SniffMimeType(kContent, kUrl, "" /* type_hint */,
                ForceSniffFileUrlsForHtml::kEnabled, &mime_type);
  EXPECT_EQ("text/html", mime_type);
}

TEST(MimeSnifferTest, UnicodeTest) {
  EXPECT_EQ("text/plain", SniffMimeType(MakeConstantString("\xEF\xBB\xBF"
                                                           "Hi there"),
                                        "http://www.example.com/foo", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString("\xEF\xBB\xBF\xED\x7A\xAD\x7A\x0D\x79"),
                    "http://www.example.com/foo", ""));
  EXPECT_EQ(
      "text/plain",
      SniffMimeType(MakeConstantString(
                        "\xFE\xFF\xD0\xA5\xD0\xBE\xD0\xBB\xD1\x83\xD0\xB9"),
                    "http://www.example.com/foo", ""));
  EXPECT_EQ("text/plain",
            SniffMimeType(
                MakeConstantString(
                    "\xFE\xFF\x00\x41\x00\x20\xD8\x00\xDC\x00\xD8\x00\xDC\x01"),
                "http://www.example.com/foo", ""));
}

TEST(MimeSnifferTest, FlashTest) {
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("CWSdd\x00\xB3"),
                          "http://www.example.com/foo", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("FLVjdkl*(#)0sdj\x00"),
                          "http://www.example.com/foo?q=ttt.swf", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("FWS3$9\r\b\x00"),
                          "http://www.example.com/foo#ttt.swf", ""));
  EXPECT_EQ("text/plain", SniffMimeType(MakeConstantString("FLVjdkl*(#)0sdj"),
                                        "http://www.example.com/foo.swf", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("FLVjdkl*(#)0s\x01dj"),
                          "http://www.example.com/foo/bar.swf", ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("FWS3$9\r\b\x1A"),
                          "http://www.example.com/foo.swf?clickTAG=http://"
                          "www.adnetwork.com/bar",
                          ""));
  EXPECT_EQ("application/octet-stream",
            SniffMimeType(MakeConstantString("FWS3$9\r\x1C\b"),
                          "http://www.example.com/foo.swf?clickTAG=http://"
                          "www.adnetwork.com/bar",
                          "text/plain"));
}

TEST(MimeSnifferTest, XMLTest) {
  // An easy feed to identify.
  EXPECT_EQ("application/atom+xml",
            SniffMimeType("<?xml?><feed", "", "text/xml"));
  // Don't sniff out of plain text.
  EXPECT_EQ("text/plain", SniffMimeType("<?xml?><feed", "", "text/plain"));
  // Simple RSS.
  EXPECT_EQ("application/rss+xml",
            SniffMimeType("<?xml version='1.0'?>\r\n<rss", "", "text/xml"));

  // The top of CNN's RSS feed, which we'd like to recognize as RSS.
  static const char kCNNRSS[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<?xml-stylesheet href=\"http://rss.cnn.com/~d/styles/rss2full.xsl\" "
      "type=\"text/xsl\" media=\"screen\"?>"
      "<?xml-stylesheet href=\"http://rss.cnn.com/~d/styles/itemcontent.css\" "
      "type=\"text/css\" media=\"screen\"?>"
      "<rss xmlns:feedburner=\"http://rssnamespace.org/feedburner/ext/1.0\" "
      "version=\"2.0\">";
  // CNN's RSS
  EXPECT_EQ("application/rss+xml", SniffMimeType(kCNNRSS, "", "text/xml"));
  EXPECT_EQ("text/plain", SniffMimeType(kCNNRSS, "", "text/plain"));

  // Don't sniff random XML as something different.
  EXPECT_EQ("text/xml", SniffMimeType("<?xml?><notafeed", "", "text/xml"));
  // Don't sniff random plain-text as something different.
  EXPECT_EQ("text/plain", SniffMimeType("<?xml?><notafeed", "", "text/plain"));

  // We never upgrade to application/xhtml+xml.
  EXPECT_EQ("text/xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">", "",
                          "text/xml"));
  EXPECT_EQ("application/xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">", "",
                          "application/xml"));
  EXPECT_EQ("text/plain",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">", "",
                          "text/plain"));
  EXPECT_EQ("application/rss+xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">", "",
                          "application/rss+xml"));
  EXPECT_EQ("text/xml", SniffMimeType("<html><head>", "", "text/xml"));
  EXPECT_EQ("text/xml",
            SniffMimeType("<foo><rss "
                          "xmlns:feedburner=\"http://rssnamespace.org/"
                          "feedburner/ext/1.0\" version=\"2.0\">",
                          "", "text/xml"));
}

// Test content which is >= 1024 bytes, and includes no open angle bracket.
// http://code.google.com/p/chromium/issues/detail?id=3521
TEST(MimeSnifferTest, XMLTestLargeNoAngledBracket) {
  // Make a large input, with 1024 bytes of "x".
  std::string content;
  content.resize(1024);
  std::fill(content.begin(), content.end(), 'x');

  // content.size() >= 1024 so the sniff is unambiguous.
  std::string mime_type;
  EXPECT_TRUE(SniffMimeType(content, GURL(), "text/xml",
                            ForceSniffFileUrlsForHtml::kDisabled, &mime_type));
  EXPECT_EQ("text/xml", mime_type);
}

// Test content which is >= 1024 bytes, and includes a binary looking byte.
// http://code.google.com/p/chromium/issues/detail?id=15314
TEST(MimeSnifferTest, LooksBinary) {
  // Make a large input, with 1024 bytes of "x" and 1 byte of 0x01.
  std::string content;
  content.resize(1024);
  std::fill(content.begin(), content.end(), 'x');
  content[1000] = 0x01;

  // content.size() >= 1024 so the sniff is unambiguous.
  std::string mime_type;
  EXPECT_TRUE(SniffMimeType(content, GURL(), "text/plain",
                            ForceSniffFileUrlsForHtml::kDisabled, &mime_type));
  EXPECT_EQ("application/octet-stream", mime_type);
}

TEST(MimeSnifferTest, OfficeTest) {
    // Check for URLs incorrectly reported as Microsoft Office files.
    EXPECT_EQ(
        "application/octet-stream",
        SniffMimeType(MakeConstantString("Hi there"),
                      "http://www.example.com/foo.doc", "application/msword"));
    EXPECT_EQ("application/octet-stream",
              SniffMimeType(MakeConstantString("Hi there"),
                            "http://www.example.com/foo.xls",
                            "application/vnd.ms-excel"));
    EXPECT_EQ("application/octet-stream",
              SniffMimeType(MakeConstantString("Hi there"),
                            "http://www.example.com/foo.ppt",
                            "application/vnd.ms-powerpoint"));
    // Check for Microsoft Office files incorrectly reported as text.
    EXPECT_EQ(
        "application/msword",
        SniffMimeType(MakeConstantString("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"
                                         "Hi there"),
                      "http://www.example.com/foo.doc", "text/plain"));
    EXPECT_EQ(
        "application/vnd.openxmlformats-officedocument."
        "wordprocessingml.document",
        SniffMimeType(MakeConstantString(

                          "PK\x03\x04"
                          "Hi there"),
                      "http://www.example.com/foo.doc", "text/plain"));
    EXPECT_EQ(
        "application/vnd.ms-excel",
        SniffMimeType(MakeConstantString("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"
                                         "Hi there"),
                      "http://www.example.com/foo.xls", "text/plain"));
    EXPECT_EQ(
        "application/vnd.openxmlformats-officedocument."
        "spreadsheetml.sheet",
        SniffMimeType(MakeConstantString("PK\x03\x04"
                                         "Hi there"),
                      "http://www.example.com/foo.xls", "text/plain"));
    EXPECT_EQ(
        "application/vnd.ms-powerpoint",
        SniffMimeType(MakeConstantString("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"
                                         "Hi there"),
                      "http://www.example.com/foo.ppt", "text/plain"));
    EXPECT_EQ(
        "application/vnd.openxmlformats-officedocument."
        "presentationml.presentation",
        SniffMimeType(MakeConstantString("PK\x03\x04"
                                         "Hi there"),
                      "http://www.example.com/foo.ppt", "text/plain"));
}

TEST(MimeSnifferTest, AudioVideoTest) {
  std::string mime_type;
  const char kOggTestData[] = "OggS\x00";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kOggTestData, sizeof(kOggTestData) - 1), &mime_type));
  EXPECT_EQ("audio/ogg", mime_type);
  mime_type.clear();
  // Check ogg header requires the terminal '\0' to be sniffed.
  EXPECT_FALSE(SniffMimeTypeFromLocalData(
      std::string_view(kOggTestData, sizeof(kOggTestData) - 2), &mime_type));
  EXPECT_EQ("", mime_type);
  mime_type.clear();

  const char kFlacTestData[] =
      "fLaC\x00\x00\x00\x22\x12\x00\x12\x00\x00\x00\x00\x00";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kFlacTestData, sizeof(kFlacTestData) - 1), &mime_type));
  EXPECT_EQ("audio/x-flac", mime_type);
  mime_type.clear();

  const char kWMATestData[] =
      "\x30\x26\xb2\x75\x8e\x66\xcf\x11\xa6\xd9\x00\xaa\x00\x62\xce\x6c";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kWMATestData, sizeof(kWMATestData) - 1), &mime_type));
  EXPECT_EQ("video/x-ms-asf", mime_type);
  mime_type.clear();

  // mp4a, m4b, m4p, and alac extension files which share the same container
  // format.
  const char kMP4TestData[] =
      "\x00\x00\x00\x20\x66\x74\x79\x70\x4d\x34\x41\x20\x00\x00\x00\x00";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kMP4TestData, sizeof(kMP4TestData) - 1), &mime_type));
  EXPECT_EQ("video/mp4", mime_type);
  mime_type.clear();

  const char kAACTestData[] =
      "\xff\xf1\x50\x80\x02\x20\xb0\x23\x0a\x83\x20\x7d\x61\x90\x3e\xb1";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kAACTestData, sizeof(kAACTestData) - 1), &mime_type));
  EXPECT_EQ("audio/mpeg", mime_type);
  mime_type.clear();

  const char kAMRTestData[] =
      "\x23\x21\x41\x4d\x52\x0a\x3c\x53\x0a\x7c\xe8\xb8\x41\xa5\x80\xca";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kAMRTestData, sizeof(kAMRTestData) - 1), &mime_type));
  EXPECT_EQ("audio/amr", mime_type);
  mime_type.clear();
}

TEST(MimeSnifferTest, ImageTest) {
  std::string mime_type;
  const char kWebPSimpleFormat[] = "RIFF\xee\x81\x00\x00WEBPVP8 ";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kWebPSimpleFormat, sizeof(kWebPSimpleFormat) - 1),
      &mime_type));
  EXPECT_EQ("image/webp", mime_type);
  mime_type.clear();

  const char kWebPLosslessFormat[] = "RIFF\xee\x81\x00\x00WEBPVP8L";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kWebPLosslessFormat, sizeof(kWebPLosslessFormat) - 1),
      &mime_type));
  EXPECT_EQ("image/webp", mime_type);
  mime_type.clear();

  const char kWebPExtendedFormat[] = "RIFF\xee\x81\x00\x00WEBPVP8X";
  EXPECT_TRUE(SniffMimeTypeFromLocalData(
      std::string_view(kWebPExtendedFormat, sizeof(kWebPExtendedFormat) - 1),
      &mime_type));
  EXPECT_EQ("image/webp", mime_type);
  mime_type.clear();
}

// The tests need char parameters, but the ranges to test include 0xFF, and some
// platforms have signed chars and are noisy about it. Using an int parameter
// and casting it to char inside the test case solves both these problems.
class MimeSnifferBinaryTest : public ::testing::TestWithParam<int> {};

// From https://mimesniff.spec.whatwg.org/#binary-data-byte :
// A binary data byte is a byte in the range 0x00 to 0x08 (NUL to BS), the byte
// 0x0B (VT), a byte in the range 0x0E to 0x1A (SO to SUB), or a byte in the
// range 0x1C to 0x1F (FS to US).
TEST_P(MimeSnifferBinaryTest, IsBinaryControlCode) {
  std::string param(1, static_cast<char>(GetParam()));
  EXPECT_TRUE(LooksLikeBinary(param));
}

// ::testing::Range(a, b) tests an open-ended range, ie. "b" is not included.
INSTANTIATE_TEST_SUITE_P(MimeSnifferBinaryTestRange1,
                         MimeSnifferBinaryTest,
                         Range(0x00, 0x09));

INSTANTIATE_TEST_SUITE_P(MimeSnifferBinaryTestByte0x0B,
                         MimeSnifferBinaryTest,
                         Values(0x0B));

INSTANTIATE_TEST_SUITE_P(MimeSnifferBinaryTestRange2,
                         MimeSnifferBinaryTest,
                         Range(0x0E, 0x1B));

INSTANTIATE_TEST_SUITE_P(MimeSnifferBinaryTestRange3,
                         MimeSnifferBinaryTest,
                         Range(0x1C, 0x20));

class MimeSnifferPlainTextTest : public ::testing::TestWithParam<int> {};

TEST_P(MimeSnifferPlainTextTest, NotBinaryControlCode) {
  std::string param(1, static_cast<char>(GetParam()));
  EXPECT_FALSE(LooksLikeBinary(param));
}

INSTANTIATE_TEST_SUITE_P(MimeSnifferPlainTextTestPlainTextControlCodes,
                         MimeSnifferPlainTextTest,
                         Values(0x09, 0x0A, 0x0C, 0x0D, 0x1B));

INSTANTIATE_TEST_SUITE_P(MimeSnifferPlainTextTestNotControlCodeRange,
                         MimeSnifferPlainTextTest,
                         Range(0x20, 0x100));

class MimeSnifferControlCodesEdgeCaseTest
    : public ::testing::TestWithParam<const char*> {};

TEST_P(MimeSnifferControlCodesEdgeCaseTest, EdgeCase) {
  EXPECT_TRUE(LooksLikeBinary(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(MimeSnifferControlCodesEdgeCaseTest,
                         MimeSnifferControlCodesEdgeCaseTest,
                         Values("\x01__",  // first byte is binary
                                "__\x03",  // last byte is binary
                                "_\x02_"   // a byte in the middle is binary
                                ));

}  // namespace
}  // namespace net
