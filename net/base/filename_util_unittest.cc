// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

struct FileCase {
  const wchar_t* file;  // nullptr indicates expected to fail.
  const char* url;
};

struct GenerateFilenameCase {
  int lineno;
  const char* url;
  const char* content_disp_header;
  const char* referrer_charset;
  const char* suggested_filename;
  const char* mime_type;
  const wchar_t* default_filename;
  const wchar_t* expected_filename;
};

// The expected filenames are coded as wchar_t for convenience.
// TODO(crbug.com/40605133): Make these char16_t once std::u16string is
// std::u16string.
std::wstring FilePathAsWString(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return path.value();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::UTF8ToWide(path.value());
#endif
}
base::FilePath WStringAsFilePath(const std::wstring& str) {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(str);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::FilePath(base::WideToUTF8(str));
#endif
}

std::string GetLocaleWarningString() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return "";
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // The generate filename tests can fail on certain OS_POSIX platforms when
  // LC_CTYPE is not "utf8" or "utf-8" because some of the string conversions
  // fail.
  // This warning text is appended to any test failures to save people time if
  // this happens to be the cause of failure :)
  // Note: some platforms (MACOSX, Chromecast) don't have this problem:
  // setlocale returns "c" but it functions as utf8.  And Android doesn't
  // have setlocale at all.
  std::string locale = setlocale(LC_CTYPE, nullptr);
  return " this test may have failed because the current LC_CTYPE locale is "
         "not utf8 (currently set to " +
         locale + ")";
#endif
}

void RunGenerateFileNameTestCase(const GenerateFilenameCase* test_case) {
  std::string default_filename(base::WideToUTF8(test_case->default_filename));
  base::FilePath file_path = GenerateFileName(
      GURL(test_case->url), test_case->content_disp_header,
      test_case->referrer_charset, test_case->suggested_filename,
      test_case->mime_type, default_filename);
  EXPECT_EQ(test_case->expected_filename, FilePathAsWString(file_path))
      << "test case at line number: " << test_case->lineno << "; "
      << GetLocaleWarningString();
}

constexpr const base::FilePath::CharType* kSafePortableBasenames[] = {
    FILE_PATH_LITERAL("a"),           FILE_PATH_LITERAL("a.txt"),
    FILE_PATH_LITERAL("a b.txt"),     FILE_PATH_LITERAL("a-b.txt"),
    FILE_PATH_LITERAL("My Computer"),
};

constexpr const base::FilePath::CharType* kUnsafePortableBasenames[] = {
    FILE_PATH_LITERAL(""),
    FILE_PATH_LITERAL("."),
    FILE_PATH_LITERAL(".."),
    FILE_PATH_LITERAL("..."),
    FILE_PATH_LITERAL("con"),
    FILE_PATH_LITERAL("con.zip"),
    FILE_PATH_LITERAL("NUL"),
    FILE_PATH_LITERAL("NUL.zip"),
    FILE_PATH_LITERAL(".a"),
    FILE_PATH_LITERAL("a."),
    FILE_PATH_LITERAL("a\"a"),
    FILE_PATH_LITERAL("a<a"),
    FILE_PATH_LITERAL("a>a"),
    FILE_PATH_LITERAL("a?a"),
    FILE_PATH_LITERAL("a/"),
    FILE_PATH_LITERAL("a\\"),
    FILE_PATH_LITERAL("a "),
    FILE_PATH_LITERAL("a . ."),
    FILE_PATH_LITERAL(" Computer"),
    FILE_PATH_LITERAL("My Computer.{a}"),
    FILE_PATH_LITERAL("My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}"),
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    FILE_PATH_LITERAL("a\\a"),
#endif
};

constexpr const base::FilePath::CharType* kUnsafePortableBasenamesForWin[] = {
    FILE_PATH_LITERAL("con"), FILE_PATH_LITERAL("con.zip"),
    FILE_PATH_LITERAL("NUL"), FILE_PATH_LITERAL("NUL.zip"),
};

constexpr const base::FilePath::CharType* kSafePortableRelativePaths[] = {
    FILE_PATH_LITERAL("a/a"),
#if BUILDFLAG(IS_WIN)
    FILE_PATH_LITERAL("a\\a"),
#endif
};

}  // namespace

TEST(FilenameUtilTest, IsSafePortablePathComponent) {
  for (auto* basename : kSafePortableBasenames) {
    EXPECT_TRUE(IsSafePortablePathComponent(base::FilePath(basename)))
        << basename;
  }
  for (auto* basename : kUnsafePortableBasenames) {
    EXPECT_FALSE(IsSafePortablePathComponent(base::FilePath(basename)))
        << basename;
  }
  for (auto* path : kSafePortableRelativePaths) {
    EXPECT_FALSE(IsSafePortablePathComponent(base::FilePath(path))) << path;
  }
}

TEST(FilenameUtilTest, IsSafePortableRelativePath) {
  base::FilePath safe_dirname(FILE_PATH_LITERAL("a"));
  for (auto* basename : kSafePortableBasenames) {
    EXPECT_TRUE(IsSafePortableRelativePath(base::FilePath(basename)))
        << basename;
    EXPECT_TRUE(IsSafePortableRelativePath(
        safe_dirname.Append(base::FilePath(basename))))
        << basename;
  }
  for (auto* path : kSafePortableRelativePaths) {
    EXPECT_TRUE(IsSafePortableRelativePath(base::FilePath(path))) << path;
    EXPECT_TRUE(
        IsSafePortableRelativePath(safe_dirname.Append(base::FilePath(path))))
        << path;
  }
  for (auto* basename : kUnsafePortableBasenames) {
    EXPECT_FALSE(IsSafePortableRelativePath(base::FilePath(basename)))
        << basename;
    if (!base::FilePath::StringType(basename).empty()) {
      EXPECT_FALSE(IsSafePortableRelativePath(
          safe_dirname.Append(base::FilePath(basename))))
          << basename;
    }
  }
}

TEST(FilenameUtilTest, FileURLConversion) {
  // a list of test file names and the corresponding URLs
  const FileCase round_trip_cases[] = {
#if BUILDFLAG(IS_WIN)
    {L"C:\\foo\\bar.txt", "file:///C:/foo/bar.txt"},
    {L"\\\\some computer\\foo\\bar.txt",
     "file://some%20computer/foo/bar.txt"},  // UNC
    {L"D:\\Name;with%some symbols*#",
     "file:///D:/Name%3Bwith%25some%20symbols*%23"},
    // issue 14153: To be tested with the OS default codepage other than 1252.
    {L"D:\\latin1\\caf\x00E9\x00DD.txt",
     "file:///D:/latin1/caf%C3%A9%C3%9D.txt"},
    {L"D:\\otherlatin\\caf\x0119.txt", "file:///D:/otherlatin/caf%C4%99.txt"},
    {L"D:\\greek\\\x03B1\x03B2\x03B3.txt",
     "file:///D:/greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"D:\\Chinese\\\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///D:/Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD%91"
     "%E9%A1%B5.doc"},
    {L"D:\\plane1\\\xD835\xDC00\xD835\xDC01.txt",  // Math alphabet "AB"
     "file:///D:/plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
    // Other percent-encoded characters that are left alone when displaying a
    // URL are decoded in a file path (https://crbug.com/585422).
    {L"C:\\foo\\\U0001F512.txt",
     "file:///C:/foo/%F0%9F%94%92.txt"},                         // Blocked.
    {L"C:\\foo\\\u2001.txt", "file:///C:/foo/%E2%80%81.txt"},    // Blocked.
    {L"C:\\foo\\\a\tbar\n ", "file:///C:/foo/%07%09bar%0A%20"},  // Blocked.
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/BAR.txt", "file:///foo/BAR.txt"},
    {L"/C:/foo/bar.txt", "file:///C:/foo/bar.txt"},
    {L"/foo/bar?.txt", "file:///foo/bar%3F.txt"},
    {L"/foo/\a\tbar\n ", "file:///foo/%07%09bar%0A%20"},
    // %5C ('\\') is not special on POSIX, and is therefore decoded as normal.
    {L"/foo/..\\bar", "file:///foo/..%5Cbar"},
    {L"/some computer/foo/bar.txt", "file:///some%20computer/foo/bar.txt"},
    {L"/Name;with%some symbols*#", "file:///Name%3Bwith%25some%20symbols*%23"},
    {L"/latin1/caf\x00E9\x00DD.txt", "file:///latin1/caf%C3%A9%C3%9D.txt"},
    {L"/otherlatin/caf\x0119.txt", "file:///otherlatin/caf%C4%99.txt"},
    {L"/greek/\x03B1\x03B2\x03B3.txt", "file:///greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"/Chinese/\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD"
     "%91%E9%A1%B5.doc"},
    {L"/plane1/\x1D400\x1D401.txt",  // Math alphabet "AB"
     "file:///plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
    // Other percent-encoded characters that are left alone when displaying a
    // URL are decoded in a file path (https://crbug.com/585422).
    {L"/foo/\U0001F512.txt", "file:///foo/%F0%9F%94%92.txt"},  // Blocked.
    {L"/foo/\u2001.txt", "file:///foo/%E2%80%81.txt"},         // Blocked.
#endif
  };

  // First, we'll test that we can round-trip all of the above cases of URLs
  base::FilePath output;
  for (const auto& test_case : round_trip_cases) {
    // convert to the file URL
    GURL file_url(FilePathToFileURL(WStringAsFilePath(test_case.file)));
    EXPECT_EQ(test_case.url, file_url.spec());

    // Back to the filename.
    EXPECT_TRUE(FileURLToFilePath(file_url, &output));
    EXPECT_EQ(test_case.file, FilePathAsWString(output));
  }

  // Test that various file: URLs get decoded into the correct file type
  FileCase url_cases[] = {
    {nullptr, "http://foo/bar.txt"},
    {nullptr, "http://localhost/foo/bar.txt"},
    {nullptr, "https://localhost/foo/bar.txt"},
#if BUILDFLAG(IS_WIN)
    {L"C:\\foo\\bar.txt", "file:c|/foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:/c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:///c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file:////foo\\bar.txt"},
    {L"\\\\foo\\bar.txt", "file:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    // %2F ('/') should fail, because it might otherwise be interpreted as a
    // path separator on Windows.
    {nullptr, "file:///C:\\foo%2f..\\bar"},
    // %5C ('\\') should fail, because it can't be represented in a Windows
    // filename (and should not be considered a path separator).
    {nullptr, "file:///foo\\..%5cbar"},
    // %00 should fail, because it represents a null byte in a filename.
    {nullptr, "file:///foo/%00bar.txt"},
    // Other percent-encoded characters that are left alone when displaying a
    // URL are decoded in a file path (https://crbug.com/585422).
    {L"C:\\foo\\\n.txt", "file:///c:/foo/%0A.txt"},         // Control char.
    {L"C:\\foo\\a=$b.txt", "file:///c:/foo/a%3D%24b.txt"},  // Reserved.
    // Make sure that '+' isn't converted into ' '.
    {L"C:\\foo\\romeo+juliet.txt", "file:/c:/foo/romeo+juliet.txt"},
    // SAMBA share case.
    {L"\\\\computername\\ShareName\\Path\\Foo.txt",
     "file://computername/ShareName/Path/Foo.txt"},
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    {L"/c:/foo/bar.txt", "file:/c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:///c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:foo/bar.txt"},
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo//bar.txt"},
    {L"/foo/bar.txt", "file:////foo///bar.txt"},
    {L"/foo/bar.txt", "file:////foo////bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:c:/foo/bar.txt"},
    // %2F ('/') should fail, because it can't be represented in a POSIX
    // filename (and should not be considered a path separator).
    {nullptr, "file:///foo%2f../bar"},
    // %00 should fail, because it represents a null byte in a filename.
    {nullptr, "file:///foo/%00bar.txt"},
    // Other percent-encoded characters that are left alone when displaying a
    // URL are decoded in a file path (https://crbug.com/585422).
    {L"/foo/\n.txt", "file:///foo/%0A.txt"},         // Control char.
    {L"/foo/a=$b.txt", "file:///foo/a%3D%24b.txt"},  // Reserved.
    // Make sure that '+' isn't converted into ' '.
    {L"/foo/romeo+juliet.txt", "file:///foo/romeo+juliet.txt"},
    // Backslashes in a file URL are normalized as forward slashes.
    {L"/bar.txt", "file://\\bar.txt"},
    {L"/c|/foo/bar.txt", "file:c|/foo\\bar.txt"},
    {L"/foo/bar.txt", "file:////foo\\bar.txt"},
    // Accept obviously-local file URLs.
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/bar.txt", "file://localhost/foo/bar.txt"},
    {L"/foo/bar.txt", "file://127.0.0.1/foo/bar.txt"},
    {L"/foo/bar.txt", "file://[::1]/foo/bar.txt"},
    // Reject non-local file URLs.
    {nullptr, "file://foo/bar.txt"},
    {nullptr, "file://example.com/bar.txt"},
    {nullptr, "file://192.168.1.1/foo/bar.txt"},
    {nullptr, "file://[2001:0db8:85a3:0000:0000:8a2e:0370:7334]/foo/bar.txt"},
#endif
  };
  for (const auto& test_case : url_cases) {
    EXPECT_EQ(test_case.file != nullptr,
              FileURLToFilePath(GURL(test_case.url), &output));
    if (test_case.file) {
      EXPECT_EQ(test_case.file, FilePathAsWString(output));
    } else {
      EXPECT_EQ(L"", FilePathAsWString(output));
    }
  }

  // Invalid UTF-8 tests can't be tested above because FilePathAsWString assumes
  // the output is valid UTF-8.

  // Invalid UTF-8 bytes in input.
  {
    const char invalid_utf8[] = "file:///d:/Blah/\x85\x99.doc";
    EXPECT_TRUE(FileURLToFilePath(GURL(invalid_utf8), &output));
#if BUILDFLAG(IS_WIN)
    // On Windows, invalid UTF-8 bytes are interpreted using the default ANSI
    // code page. This defaults to Windows-1252 (which we assume here).
    const base::FilePath::CharType expected_output[] =
        FILE_PATH_LITERAL("D:\\Blah\\\u2026\u2122.doc");
    EXPECT_EQ(expected_output, output.value());
#elif BUILDFLAG(IS_POSIX)
    // No conversion should happen, and the invalid UTF-8 should be preserved.
    const char expected_output[] = "/d:/Blah/\x85\x99.doc";
    EXPECT_EQ(expected_output, output.value());
#endif
  }

  // Invalid UTF-8 percent-encoded bytes in input.
  {
    const char invalid_utf8[] = "file:///d:/Blah/%85%99.doc";
    EXPECT_TRUE(FileURLToFilePath(GURL(invalid_utf8), &output));
#if BUILDFLAG(IS_WIN)
    // On Windows, invalid UTF-8 bytes are interpreted using the default ANSI
    // code page. This defaults to Windows-1252 (which we assume here).
    const base::FilePath::CharType expected_output[] =
        FILE_PATH_LITERAL("D:\\Blah\\\u2026\u2122.doc");
    EXPECT_EQ(expected_output, output.value());
#elif BUILDFLAG(IS_POSIX)
    // No conversion should happen, and the invalid UTF-8 should be preserved.
    const char expected_output[] = "/d:/Blah/\x85\x99.doc";
    EXPECT_EQ(expected_output, output.value());
#endif
  }

  // Test that if a file URL is malformed, we get a failure
  EXPECT_FALSE(FileURLToFilePath(GURL("filefoobar"), &output));
}

TEST(FilenameUtilTest, GenerateSafeFileName) {
  const struct {
    int line;
    const char* mime_type;
    const char* filename;
    const char* expected_filename;
  } safe_tests[] = {
    {__LINE__, "text/html", "bar.htm", "bar.htm"},
    {__LINE__, "text/html", "bar.html", "bar.html"},
    {__LINE__, "application/x-chrome-extension", "bar", "bar.crx"},
    {__LINE__, "image/png", "bar.html", "bar.html"},
    {__LINE__, "text/html", "bar.exe", "bar.exe"},
    {__LINE__, "image/gif", "bar.exe", "bar.exe"},
    {__LINE__, "text/html", "google.com", "google.com"},
    // Allow extension synonyms.
    {__LINE__, "image/jpeg", "bar.jpg", "bar.jpg"},
    {__LINE__, "image/jpeg", "bar.jpeg", "bar.jpeg"},

#if BUILDFLAG(IS_WIN)
    // Device names
    {__LINE__, "text/html", "con.htm", "_con.htm"},
    {__LINE__, "text/html", "lpt1.htm", "_lpt1.htm"},
    {__LINE__, "application/x-chrome-extension", "con", "_con.crx"},

    // Looks like foo.{GUID} which get treated as namespace mounts on Windows.
    {__LINE__, "text/html", "harmless.{not-really-this-may-be-a-guid}",
     "harmless.download"},
    {__LINE__, "text/html", "harmless.{mismatched-", "harmless.{mismatched-"},

    // Dangerous extensions
    {__LINE__, "text/html", "harmless.local", "harmless.download"},
    {__LINE__, "text/html", "harmless.lnk", "harmless.download"},
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // On Posix, none of the above set is particularly dangerous.
    {__LINE__, "text/html", "con.htm", "con.htm"},
    {__LINE__, "text/html", "lpt1.htm", "lpt1.htm"},
    {__LINE__, "application/x-chrome-extension", "con", "con.crx"},
    {__LINE__, "text/html", "harmless.{not-really-this-may-be-a-guid}",
     "harmless.{not-really-this-may-be-a-guid}"},
    {__LINE__, "text/html", "harmless.{mismatched-", "harmless.{mismatched-"},
    {__LINE__, "text/html", "harmless.local", "harmless.local"},
    {__LINE__, "text/html", "harmless.lnk", "harmless.lnk"},
#endif  // BUILDFLAG(IS_WIN)
  };

#if BUILDFLAG(IS_WIN)
  base::FilePath base_path(FILE_PATH_LITERAL("C:\\foo"));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::FilePath base_path("/foo");
#endif

  for (const auto& test : safe_tests) {
    base::FilePath file_path = base_path.AppendASCII(test.filename);
    base::FilePath expected_path =
        base_path.AppendASCII(test.expected_filename);
    GenerateSafeFileName(test.mime_type, false, &file_path);
    EXPECT_EQ(expected_path.value(), file_path.value())
        << "Test case at line " << test.line;
  }
}

TEST(FilenameUtilTest, GenerateFileName_Assumptions) {
  base::FilePath::StringType extension;
  EXPECT_TRUE(GetPreferredExtensionForMimeType("application/x-chrome-extension",
                                               &extension));
  EXPECT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("crx")), extension);
}

TEST(FilenameUtilTest, GenerateFileName) {
  // Tests whether the correct filename is selected from the the given
  // parameters and that Content-Disposition headers are properly
  // handled including failovers when the header is malformed.
  const GenerateFilenameCase selection_tests[] = {
      {// Picks the filename from the C-D header.
       __LINE__, "http://www.google.com/", "attachment; filename=test.html", "",
       "", "", L"", L"test.html"},
      {// Ditto. The C-D header uses a quoted string.
       __LINE__, "http://www.google.com/", "attachment; filename=\"test.html\"",
       "", "", "", L"", L"test.html"},
      {// Ditto. Extra whilespace after the '=' sign.
       __LINE__, "http://www.google.com/",
       "attachment; filename= \"test.html\"", "", "", "", L"", L"test.html"},
      {// Ditto. Whitespace before and after '=' sign.
       __LINE__, "http://www.google.com/",
       "attachment; filename   =   \"test.html\"", "", "", "", L"",
       L"test.html"},
      {// Filename is whitespace.  Should failover to URL host
       __LINE__, "http://www.google.com/", "attachment; filename=  ", "", "",
       "", L"", L"www.google.com"},
      {// No filename.
       __LINE__, "http://www.google.com/path/test.html", "attachment", "", "",
       "", L"", L"test.html"},
      {// Ditto
       __LINE__, "http://www.google.com/path/test.html", "attachment;", "", "",
       "", L"", L"test.html"},
      {// No C-D, and no URL path.
       __LINE__, "http://www.google.com/", "", "", "", "", L"",
       L"www.google.com"},
      {// No C-D. URL has a path.
       __LINE__, "http://www.google.com/test.html", "", "", "", "", L"",
       L"test.html"},
      {// No C-D. URL's path ends in a slash which results in an empty final
       // component.
       __LINE__, "http://www.google.com/path/", "", "", "", "", L"",
       L"www.google.com"},
      {// No C-D. URL has a path, but the path has no extension.
       __LINE__, "http://www.google.com/path", "", "", "", "", L"", L"path"},
      {// No C-D. URL gives no filename hints.
       __LINE__, "file:///", "", "", "", "", L"", L"download"},
      {// file:// URL.
       __LINE__, "file:///path/testfile", "", "", "", "", L"", L"testfile"},
      {// Unknown scheme.
       __LINE__, "non-standard-scheme:", "", "", "", "", L"", L"download"},
      {// C-D overrides default
       __LINE__, "http://www.google.com/",
       "attachment; filename =\"test.html\"", "", "", "", L"download",
       L"test.html"},
      {// But the URL doesn't
       __LINE__, "http://www.google.com/", "", "", "", "", L"download",
       L"download"},
      // Below is a small subset of cases taken from HttpContentDisposition
      // tests.
      {__LINE__, "http://www.google.com/",
       "attachment; filename=\"%EC%98%88%EC%88%A0%20"
       "%EC%98%88%EC%88%A0.jpg\"",
       "", "", "", L"", L"\uc608\uc220 \uc608\uc220.jpg"},
      {__LINE__,
       "http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg", "",
       "", "", "", L"download", L"\uc608\uc220 \uc608\uc220.jpg"},
      {__LINE__, "http://www.google.com/", "attachment;", "", "", "",
       L"\uB2E4\uC6B4\uB85C\uB4DC", L"\uB2E4\uC6B4\uB85C\uB4DC"},
      {__LINE__, "http://www.google.com/",
       "attachment; filename=\"=?EUC-JP?Q?=B7=DD=BD="
       "D13=2Epng?=\"",
       "", "", "", L"download", L"\u82b8\u88533.png"},
      {__LINE__, "http://www.example.com/images?id=3",
       "attachment; filename=caf\xc3\xa9.png", "iso-8859-1", "", "", L"",
       L"caf\u00e9.png"},
      {__LINE__, "http://www.example.com/images?id=3",
       "attachment; filename=caf\xe5.png", "windows-1253", "", "", L"",
       L"caf\u03b5.png"},
      {// Invalid C-D header. Name value is skipped now.
       __LINE__, "http://www.example.com/file?id=3",
       "attachment; name=\xcf\xc2\xd4\xd8.zip", "GBK", "", "", L"", L"file"},
      {// Invalid C-D header. Extracts filename from url.
       __LINE__, "http://www.google.com/test.html",
       "attachment; filename==?iiso88591?Q?caf=EG?=", "", "", "", L"",
       L"test.html"},
      // about: and data: URLs
      {__LINE__, "about:chrome", "", "", "", "", L"", L"download"},
      {__LINE__, "data:,looks/like/a.path", "", "", "", "", L"", L"download"},
      {__LINE__, "data:text/plain;base64,VG8gYmUgb3Igbm90IHRvIGJlLg=", "", "",
       "", "", L"", L"download"},
      {__LINE__, "data:,looks/like/a.path", "", "", "", "",
       L"default_filename_is_given", L"default_filename_is_given"},
      {__LINE__, "data:,looks/like/a.path", "", "", "", "",
       L"\u65e5\u672c\u8a9e",  // Japanese Kanji.
       L"\u65e5\u672c\u8a9e"},
      {// The filename encoding is specified by the referrer charset.
       __LINE__, "http://example.com/V%FDvojov%E1%20psychologie.doc", "",
       "iso-8859-1", "", "", L"", L"V\u00fdvojov\u00e1 psychologie.doc"},
      {// Suggested filename takes precedence over URL
       __LINE__, "http://www.google.com/test", "", "", "suggested", "", L"",
       L"suggested"},
      {// The content-disposition has higher precedence over the suggested name.
       __LINE__, "http://www.google.com/test", "attachment; filename=test.html",
       "", "suggested", "", L"", L"test.html"},
      {__LINE__, "http://www.google.com/test", "attachment; filename=test",
       "utf-8", "", "image/png", L"", L"test"},
      // Raw 8bit characters in C-D
      {__LINE__, "http://www.example.com/images?id=3",
       "attachment; filename=caf\xc3\xa9.png", "iso-8859-1", "", "image/png",
       L"", L"caf\u00e9.png"},
      {__LINE__, "http://www.example.com/images?id=3",
       "attachment; filename=caf\xe5.png", "windows-1253", "", "image/png", L"",
       L"caf\u03b5.png"},
      {// No 'filename' keyword in the disposition, use the URL
       __LINE__, "http://www.evil.com/my_download.txt", "a_file_name.txt", "",
       "", "text/plain", L"download", L"my_download.txt"},
      {// Spaces in the disposition file name
       __LINE__, "http://www.frontpagehacker.com/a_download.exe",
       "filename=My Downloaded File.exe", "", "", "application/octet-stream",
       L"download", L"My Downloaded File.exe"},
      {// % encoded
       __LINE__, "http://www.examples.com/",
       "attachment; "
       "filename=\"%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg\"",
       "", "", "application/x-chrome-extension", L"download",
       L"\uc608\uc220 \uc608\uc220.jpg"},
      {// Invalid C-D header. Name value is skipped now.
       __LINE__, "http://www.examples.com/q.cgi?id=abc",
       "attachment; name=abc de.pdf", "", "", "application/octet-stream",
       L"download", L"q.cgi"},
      {__LINE__, "http://www.example.com/path",
       "filename=\"=?EUC-JP?Q?=B7=DD=BD=D13=2Epng?=\"", "", "", "image/png",
       L"download",
       L"\x82b8\x8853"
       L"3.png"},
      {// The following two have invalid CD headers and filenames come from the
       // URL.
       __LINE__, "http://www.example.com/test%20123",
       "attachment; filename==?iiso88591?Q?caf=EG?=", "", "", "", L"download",
       L"test 123"},
      {__LINE__,
       "http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg",
       "malformed_disposition", "", "", "", L"download",
       L"\uc608\uc220 \uc608\uc220.jpg"},
      {// Invalid C-D. No filename from URL. Falls back to 'download'.
       __LINE__, "http://www.google.com/path1/path2/",
       "attachment; filename==?iso88591?Q?caf=E3?", "", "", "", L"download",
       L"download"},
  };

  // Tests filename generation.  Once the correct filename is
  // selected, they should be passed through the validation steps and
  // a correct extension should be added if necessary.
  const GenerateFilenameCase generation_tests[] = {
    // Dotfiles. Ensures preceeding period(s) stripped.
    {__LINE__, "http://www.google.com/.test.html", "", "", "", "", L"",
     L"test.html"},
    {__LINE__, "http://www.google.com/.test", "", "", "", "", L"", L"test"},
    {__LINE__, "http://www.google.com/..test", "", "", "", "", L"", L"test"},
    {// Disposition has relative paths, remove directory separators
     __LINE__, "", "filename=../../../../././../a_file_name.txt", "", "",
     "text/plain", L"download", L"_.._.._.._._._.._a_file_name.txt"},
    {// Disposition has parent directories, remove directory separators
     __LINE__, "", "filename=dir1/dir2/a_file_name.txt", "", "", "text/plain",
     L"download", L"dir1_dir2_a_file_name.txt"},
    {// Disposition has relative paths, remove directory separators
     __LINE__, "", "filename=..\\..\\..\\..\\.\\.\\..\\a_file_name.txt", "", "",
     "text/plain", L"download", L"_.._.._.._._._.._a_file_name.txt"},
    {// Disposition has parent directories, remove directory separators
     __LINE__, "", "filename=dir1\\dir2\\a_file_name.txt", "", "", "text/plain",
     L"download", L"dir1_dir2_a_file_name.txt"},
    {// Filename looks like HTML?
     __LINE__, "", "filename=\"<blink>Hello kitty</blink>\"", "", "",
     "text/plain", L"default", L"_blink_Hello kitty__blink_"},
    {// A normal avi should get .avi and not .avi.avi
     __LINE__, "https://example.com/misc/2.avi", "", "", "", "video/x-msvideo",
     L"download", L"2.avi"},
    {// Slashes are illegal, and should be replaced with underscores.
     __LINE__, "http://example.com/foo%2f..%2fbar.jpg", "", "", "",
     "text/plain", L"download", L"foo_.._bar.jpg"},
    {// "%00" decodes to the NUL byte, which is illegal and should be replaced
     // with an underscore. (Note: This can't be tested with a URL, since "%00"
     // is illegal in a URL. Only applies to Content-Disposition.)
     __LINE__, "http://example.com/download.py", "filename=foo%00bar.jpg", "",
     "", "text/plain", L"download", L"foo_bar.jpg"},
    {// Extension generation for C-D derived filenames.
     __LINE__, "", "filename=my-cat", "", "", "image/jpeg", L"download",
     L"my-cat"},
    {// Unknown MIME type
     __LINE__, "", "filename=my-cat", "", "", "dance/party", L"download",
     L"my-cat"},
    {// Known MIME type.
     __LINE__, "", "filename=my-cat.jpg", "", "", "text/plain", L"download",
     L"my-cat.jpg"},
#if BUILDFLAG(IS_WIN)
    // Test truncation of trailing dots and spaces (Windows)
    {__LINE__, "", "filename=evil.exe ", "", "", "binary/octet-stream",
     L"download", L"evil.exe"},
    {__LINE__, "", "filename=evil.exe.", "", "", "binary/octet-stream",
     L"download", L"evil.exe_"},
    {__LINE__, "", "filename=evil.exe.  .  .", "", "", "binary/octet-stream",
     L"download", L"evil.exe_______"},
    {__LINE__, "", "filename=evil.", "", "", "binary/octet-stream", L"download",
     L"evil_"},
    {__LINE__, "", "filename=. . . . .", "", "", "binary/octet-stream",
     L"download", L"download"},
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // Test truncation of trailing dots and spaces (non-Windows)
    {__LINE__, "", "filename=evil.exe ", "", "", "binary/octet-stream",
     L"download", L"evil.exe"},
    {__LINE__, "", "filename=evil.exe.", "", "", "binary/octet-stream",
     L"download", L"evil.exe"},
    {__LINE__, "", "filename=evil.exe.  .  .", "", "", "binary/octet-stream",
     L"download", L"evil.exe.  . _"},
    {__LINE__, "", "filename=evil.", "", "", "binary/octet-stream", L"download",
     L"evil"},
    {__LINE__, "", "filename=. . . . .", "", "", "binary/octet-stream",
     L"download", L"_. . ._"},
#endif
    {__LINE__, "", "attachment; filename=\"meh.exe\xC2\xA0\"", "", "",
     "binary/octet-stream", L"", L"meh.exe_"},
    // Disappearing directory references:
    {__LINE__, "", "filename=.", "", "", "dance/party", L"download",
     L"download"},
    {__LINE__, "", "filename=..", "", "", "dance/party", L"download",
     L"download"},
    {__LINE__, "", "filename=...", "", "", "dance/party", L"download",
     L"download"},
    // Reserved words on Windows
    {__LINE__, "", "filename=COM1", "", "", "application/foo-bar", L"download",
#if BUILDFLAG(IS_WIN)
     L"_COM1"
#else
     L"COM1"
#endif
    },
    {__LINE__, "", "filename=COM4.txt", "", "", "text/plain", L"download",
#if BUILDFLAG(IS_WIN)
     L"_COM4.txt"
#else
     L"COM4.txt"
#endif
    },
    {__LINE__, "", "filename=lpt1.TXT", "", "", "text/plain", L"download",
#if BUILDFLAG(IS_WIN)
     L"_lpt1.TXT"
#else
     L"lpt1.TXT"
#endif
    },
    {__LINE__, "", "filename=clock$.txt", "", "", "text/plain", L"download",
#if BUILDFLAG(IS_WIN)
     L"_clock$.txt"
#else
     L"clock$.txt"
#endif
    },
    {// Validation should also apply to sugested name
     __LINE__, "", "", "", "clock$.txt", "text/plain", L"download",
#if BUILDFLAG(IS_WIN)
     L"_clock$.txt"
#else
     L"clock$.txt"
#endif
    },
    {// Device names only work when present at the start of the string.
     __LINE__, "", "filename=mycom1.foo", "", "", "", L"download",
     L"mycom1.foo"},
    {__LINE__, "", "filename=Setup.exe.local", "", "", "", L"download",
#if BUILDFLAG(IS_WIN)
     L"Setup.exe.download"
#else
     L"Setup.exe.local"
#endif
    },
    {__LINE__, "", "filename=Setup.exe.local.local", "", "", "", L"download",
#if BUILDFLAG(IS_WIN)
     L"Setup.exe.local.download"
#else
     L"Setup.exe.local.local"
#endif
    },
    {__LINE__, "", "filename=Setup.exe.lnk", "", "", "", L"download",
#if BUILDFLAG(IS_WIN)
     L"Setup.exe.download"
#else
     L"Setup.exe.lnk"
#endif
    },
    {__LINE__, "", "filename=Desktop.ini", "", "", "", L"download",
#if BUILDFLAG(IS_WIN)
     L"_Desktop.ini"
#else
     L"Desktop.ini"
#endif
    },
    {__LINE__, "", "filename=Thumbs.db", "", "", "", L"download",
#if BUILDFLAG(IS_WIN)
     L"_Thumbs.db"
#else
     L"Thumbs.db"
#endif
    },

    // Regression tests for older issues:
    {// http://crbug.com/5772.
     __LINE__, "http://www.example.com/foo.tar.gz", "", "", "",
     "application/x-tar", L"download", L"foo.tar.gz"},
    {// http://crbug.com/52250.
     __LINE__, "http://www.example.com/foo.tgz", "", "", "",
     "application/x-tar", L"download", L"foo.tgz"},
    {// http://crbug.com/7337.
     __LINE__, "http://maged.lordaeron.org/blank.reg", "", "", "",
     "text/x-registry", L"download", L"blank.reg"},
    {__LINE__, "http://www.example.com/bar.tar", "", "", "",
     "application/x-tar", L"download", L"bar.tar"},
    {__LINE__, "http://www.example.com/bar.bogus", "", "", "",
     "application/x-tar", L"download", L"bar.bogus"},
    {// http://crbug.com/20337
     __LINE__, "http://www.example.com/.download.txt", "filename=.download.txt",
     "", "", "text/plain", L"-download", L"download.txt"},
    {// http://crbug.com/56855.
     __LINE__, "http://www.example.com/bar.sh", "", "", "", "application/x-sh",
     L"download", L"bar.sh"},
    {// http://crbug.com/61571
     __LINE__, "http://www.example.com/npdf.php?fn=foobar.pdf", "", "", "",
     "application/x-chrome-extension", L"download", L"npdf.crx"},
    {// Shouldn't overwrite C-D specified extension.
     __LINE__, "http://www.example.com/npdf.php?fn=foobar.pdf",
     "filename=foobar.jpg", "", "", "text/plain", L"download", L"foobar.jpg"},
    {// http://crbug.com/87719
     __LINE__, "http://www.example.com/image.aspx?id=blargh", "", "", "",
     "application/x-chrome-extension", L"download", L"image.crx"},
    {__LINE__, "http://www.example.com/image.aspx?id=blargh", "", "", " .foo",
     "", L"download", L"_.foo"},

    // Note that the next 4 tests will not fail on all platforms on regression.
    // They only fail if application/[x-]gzip has a default extension, which
    // can vary across platforms (And even by OS install).
    {__LINE__, "http://www.example.com/goat.tar.gz?wearing_hat=true", "", "",
     "", "application/gzip", L"", L"goat.tar.gz"},
    {__LINE__, "http://www.example.com/goat.tar.gz?wearing_hat=true", "", "",
     "", "application/x-gzip", L"", L"goat.tar.gz"},
    {__LINE__, "http://www.example.com/goat.tgz?wearing_hat=true", "", "", "",
     "application/gzip", L"", L"goat.tgz"},
    {__LINE__, "http://www.example.com/goat.tgz?wearing_hat=true", "", "", "",
     "application/x-gzip", L"", L"goat.tgz"},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {// http://crosbug.com/26028
     __LINE__, "http://www.example.com/fooa%cc%88.txt", "", "", "",
     "image/jpeg", L"foo\xe4", L"foo\xe4.txt"},
#endif

    // U+3000 IDEOGRAPHIC SPACE (http://crbug.com/849794): In URL file name.
    {__LINE__, "http://www.example.com/%E5%B2%A1%E3%80%80%E5%B2%A1.txt", "", "",
     "", "text/plain", L"", L"\u5ca1\u3000\u5ca1.txt"},
    // U+3000 IDEOGRAPHIC SPACE (http://crbug.com/849794): In
    // Content-Disposition filename.
    {__LINE__, "http://www.example.com/download.py",
     "filename=%E5%B2%A1%E3%80%80%E5%B2%A1.txt", "utf-8", "", "text/plain", L"",
     L"\u5ca1\u3000\u5ca1.txt"},
  };

  for (const auto& selection_test : selection_tests)
    RunGenerateFileNameTestCase(&selection_test);

  for (const auto& generation_test : generation_tests)
    RunGenerateFileNameTestCase(&generation_test);

  for (const auto& generation_test : generation_tests) {
    GenerateFilenameCase test_case = generation_test;
    test_case.referrer_charset = "GBK";
    RunGenerateFileNameTestCase(&test_case);
  }
}

TEST(FilenameUtilTest, IsReservedNameOnWindows) {
  for (auto* basename : kSafePortableBasenames) {
    EXPECT_FALSE(IsReservedNameOnWindows(base::FilePath(basename).value()))
        << basename;
  }

  for (auto* basename : kUnsafePortableBasenamesForWin) {
    EXPECT_TRUE(IsReservedNameOnWindows(base::FilePath(basename).value()))
        << basename;
  }
}

}  // namespace net
