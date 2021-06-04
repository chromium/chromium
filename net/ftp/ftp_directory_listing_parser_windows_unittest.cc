// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/cxx17_backports.h"
#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_windows.h"

namespace net {

namespace {

typedef FtpDirectoryListingParserTest FtpDirectoryListingParserWindowsTest;

TEST_F(FtpDirectoryListingParserWindowsTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "11-02-09  05:32PM       <DIR>          NT",
      FtpDirectoryListingEntry::DIRECTORY, "NT", -1,
      2009, 11, 2, 17, 32 },
    { "01-06-09  02:42PM                  458 Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2009, 1, 6, 14, 42 },
    { "01-06-09  02:42AM                  1 Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 1,
      2009, 1, 6, 2, 42 },
    { "01-06-01  02:42AM                  458 Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2001, 1, 6, 2, 42 },
    { "01-06-00  02:42AM                  458 Corner1.txt",
      FtpDirectoryListingEntry::FILE, "Corner1.txt", 458,
      2000, 1, 6, 2, 42 },
    { "01-06-99  02:42AM                  458 Corner2.txt",
      FtpDirectoryListingEntry::FILE, "Corner2.txt", 458,
      1999, 1, 6, 2, 42 },
    { "01-06-80  02:42AM                  458 Corner3.txt",
      FtpDirectoryListingEntry::FILE, "Corner3.txt", 458,
      1980, 1, 6, 2, 42 },
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // TODO(phajdan.jr): https://crbug.com/28792: Re-enable when 2038-year
    // problem is fixed on Linux.
    { "01-06-79  02:42AM                  458 Corner4",
      FtpDirectoryListingEntry::FILE, "Corner4", 458,
      2079, 1, 6, 2, 42 },
#endif  // !defined (OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    { "01-06-1979  02:42AM                458 Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      1979, 1, 6, 2, 42 },
    { "11-02-09  05:32PM       <DIR>          My Directory",
      FtpDirectoryListingEntry::DIRECTORY, "My Directory", -1,
      2009, 11, 2, 17, 32 },
    { "12-25-10  12:00AM       <DIR>          Christmas Midnight",
      FtpDirectoryListingEntry::DIRECTORY, "Christmas Midnight", -1,
      2010, 12, 25, 0, 0 },
    { "12-25-10  12:00PM       <DIR>          Christmas Midday",
      FtpDirectoryListingEntry::DIRECTORY, "Christmas Midday", -1,
      2010, 12, 25, 12, 0 },
  };
  for (size_t i = 0; i < base::size(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingWindows(
        GetSingleLineTestCase(good_cases[i].input),
        &entries));
    VerifySingleLineTestCase(good_cases[i], entries);
  }
}

TEST_F(FtpDirectoryListingParserWindowsTest, Ignored) {
  const char* const ignored_cases[] = {
    "12-07-10  12:05AM       <DIR>    ",  // http://crbug.com/66097
    "12-07-10  12:05AM       1234    ",
    "11-02-09  05:32         <DIR>",
    "11-02-09  05:32PM       <DIR>",
  };
  for (size_t i = 0; i < base::size(ignored_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    ignored_cases[i]));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingWindows(
                    GetSingleLineTestCase(ignored_cases[i]),
                    &entries));
    EXPECT_EQ(0U, entries.size());
  }
}

TEST_F(FtpDirectoryListingParserWindowsTest, Bad) {
  const char* const bad_cases[] = {
    "garbage",
    "11-02-09  05:32PM       <GARBAGE>",
    "11-02-09  05:32PM       <GARBAGE>      NT",
    "11-FEB-09 05:32PM       <DIR>",
    "11-02     05:32PM       <DIR>",
    "11-02-09  05:32PM                 -1",
    "11-FEB-09 05:32PM       <DIR>          NT",
    "11-02     05:32PM       <DIR>          NT",
    "11-02-09  05:32PM                 -1   NT",
    "99-25-10  12:00AM                  0",
    "12-99-10  12:00AM                  0",
    "12-25-10  99:00AM                  0",
    "12-25-10  12:99AM                  0",
    "12-25-10  12:00ZM                  0",
    "99-25-10  12:00AM                  0   months out of range",
    "12-99-10  12:00AM                  0   days out of range",
    "12-25-10  99:00AM                  0   hours out of range",
    "12-25-10  12:99AM                  0   minutes out of range",
    "12-25-10  12:00ZM                  0   what does ZM mean",
  };
  for (size_t i = 0; i < base::size(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    bad_cases[i]));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_FALSE(ParseFtpDirectoryListingWindows(
                     GetSingleLineTestCase(bad_cases[i]),
                     &entries));
  }
}

}  // namespace

}  // namespace net
