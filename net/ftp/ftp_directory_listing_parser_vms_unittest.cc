// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/cxx17_backports.h"
#include "base/format_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser_vms.h"

using base::ASCIIToUTF16;

namespace net {

namespace {

typedef FtpDirectoryListingParserTest FtpDirectoryListingParserVmsTest;

TEST_F(FtpDirectoryListingParserVmsTest, Good) {
  const struct SingleLineTestData good_cases[] = {
      {"README.TXT;4  2  18-APR-2000 10:40:39.90",
       FtpDirectoryListingEntry::FILE, "readme.txt", 1024, 2000, 4, 18, 10, 40},
      {".WELCOME;1    2  13-FEB-2002 23:32:40.47",
       FtpDirectoryListingEntry::FILE, ".welcome", 1024, 2002, 2, 13, 23, 32},
      {"FILE.;1    2  13-FEB-2002 23:32:40.47", FtpDirectoryListingEntry::FILE,
       "file.", 1024, 2002, 2, 13, 23, 32},
      {"EXAMPLE.TXT;1  1   4-NOV-2009 06:02 [JOHNDOE] (RWED,RWED,,)",
       FtpDirectoryListingEntry::FILE, "example.txt", 512, 2009, 11, 4, 6, 2},
      {"ANNOUNCE.TXT;2 1/16 12-MAR-2005 08:44:57 [SYSTEM] (RWED,RWED,RE,RE)",
       FtpDirectoryListingEntry::FILE, "announce.txt", 512, 2005, 3, 12, 8, 44},
      {"TEST.DIR;1 1 4-MAR-1999 22:14:34 [UCX$NOBO,ANONYMOUS] "
       "(RWE,RWE,RWE,RWE)",
       FtpDirectoryListingEntry::DIRECTORY, "test", -1, 1999, 3, 4, 22, 14},
      {"ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (,,,)",
       FtpDirectoryListingEntry::FILE, "announce.txt", 512, 2005, 3, 12, 8, 44},
      {"ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (R,RW,RWD,RE)",
       FtpDirectoryListingEntry::FILE, "announce.txt", 512, 2005, 3, 12, 8, 44},
      {"ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (ED,RED,WD,WED)",
       FtpDirectoryListingEntry::FILE, "announce.txt", 512, 2005, 3, 12, 8, 44},
      {"VMS721.ISO;2 ******  6-MAY-2008 09:29 [ANONY,ANONYMOUS] "
       "(RE,RWED,RE,RE)",
       FtpDirectoryListingEntry::FILE, "vms721.iso", -1, 2008, 5, 6, 9, 29},
      // This has an unusually large allocated block size (INT64_MAX), but
      // shouldn't matter as it is not used.
      {"ANNOUNCE.TXT;2 1/9223372036854775807 12-MAR-2005 08:44:57 [SYSTEM] "
       "(RWED,RWED,RE,RE)",
       FtpDirectoryListingEntry::FILE, "announce.txt", 512, 2005, 3, 12, 8, 44},
  };
  for (size_t i = 0; i < base::size(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    std::vector<std::u16string> lines(
        GetSingleLineTestCase(good_cases[i].input));

    // The parser requires a directory header before accepting regular input.
    lines.insert(lines.begin(), u"Directory ANONYMOUS_ROOT:[000000]");

    // A valid listing must also have a "Total" line at the end.
    lines.insert(lines.end(), u"Total of 1 file, 2 blocks.");

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingVms(lines,
                                            &entries));
    VerifySingleLineTestCase(good_cases[i], entries);
  }
}

TEST_F(FtpDirectoryListingParserVmsTest, Bad) {
  const char* const bad_cases[] = {
      "garbage",

      // Missing file version number.
      "README.TXT 2 18-APR-2000 10:40:39",

      // Missing extension.
      "README;1 2 18-APR-2000 10:40:39",

      // Malformed file size.
      "README.TXT;1 garbage 18-APR-2000 10:40:39",
      "README.TXT;1 -2 18-APR-2000 10:40:39",

      // Malformed date.
      "README.TXT;1 2 APR-2000 10:40:39",
      "README.TXT;1 2 -18-APR-2000 10:40:39", "README.TXT;1 2 18-APR 10:40:39",
      "README.TXT;1 2 18-APR-2000 10", "README.TXT;1 2 18-APR-2000 10:40.25",
      "README.TXT;1 2 18-APR-2000 10:40.25.25",

      // Malformed security information.
      "X.TXT;2 1 12-MAR-2005 08:44:57 (RWED,RWED,RE,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM]",
      "X.TXT;2 1 12-MAR-2005 08:44:57 (SYSTEM) (RWED,RWED,RE,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM] [RWED,RWED,RE,RE]",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,RE,RE,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWEDRWED,RE,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,DEWR,RE,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,Q,RE)",
      "X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RRWWEEDD,RE,RE)",

      // Block size (INT64_MAX) is too large -- will overflow when
      // multiplying by 512 to calculate the file size in bytes.
      "README.TXT;1  9223372036854775807  18-APR-2000 10:40:39.90",
      "README.TXT;1  9223372036854775807/9223372036854775807  18-APR-2000 "
      "10:40:39.90",

      // Block size (larger than INT64_MAX) is too large -- will fail to
      // parse to an int64_t
      "README.TXT;1  19223372036854775807  18-APR-2000 10:40:39.90",
      "README.TXT;1  19223372036854775807/19223372036854775807  18-APR-2000 "
      "10:40:39.90",
  };
  for (size_t i = 0; i < base::size(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i, bad_cases[i]));

    std::vector<std::u16string> lines(GetSingleLineTestCase(bad_cases[i]));

    // The parser requires a directory header before accepting regular input.
    lines.insert(lines.begin(), u"Directory ANONYMOUS_ROOT:[000000]");

    // A valid listing must also have a "Total" line at the end.
    lines.insert(lines.end(), u"Total of 1 file, 2 blocks.");

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_FALSE(ParseFtpDirectoryListingVms(lines,
                                             &entries));
  }
}

TEST_F(FtpDirectoryListingParserVmsTest, BadDataAfterFooter) {
  const char* const bad_cases[] = {
    "garbage",
    "Total of 1 file, 2 blocks.",
    "Directory ANYNYMOUS_ROOT:[000000]",
  };
  for (size_t i = 0; i < base::size(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i, bad_cases[i]));

    std::vector<std::u16string> lines(
        GetSingleLineTestCase("README.TXT;4  2  18-APR-2000 10:40:39.90"));

    // The parser requires a directory header before accepting regular input.
    lines.insert(lines.begin(), u"Directory ANONYMOUS_ROOT:[000000]");

    // A valid listing must also have a "Total" line at the end.
    lines.insert(lines.end(), u"Total of 1 file, 2 blocks.");

    {
      // Make sure the listing is valid before we add data after footer.
      std::vector<FtpDirectoryListingEntry> entries;
      EXPECT_TRUE(ParseFtpDirectoryListingVms(lines,
                                              &entries));
    }

    {
      // Insert a line at the end of the listing that should make it invalid.
      lines.insert(lines.end(),
                   ASCIIToUTF16(bad_cases[i]));
      std::vector<FtpDirectoryListingEntry> entries;
      EXPECT_FALSE(ParseFtpDirectoryListingVms(lines,
                                               &entries));
    }
  }
}

TEST_F(FtpDirectoryListingParserVmsTest, EmptyColumnZero) {
  std::vector<std::u16string> lines;

  // The parser requires a directory header before accepting regular input.
  lines.push_back(u"garbage");

  char16_t data[] = {0x0};
  lines.push_back(std::u16string(data, 1));

  std::vector<FtpDirectoryListingEntry> entries;
  EXPECT_FALSE(ParseFtpDirectoryListingVms(lines, &entries));
}

TEST_F(FtpDirectoryListingParserVmsTest, EmptyColumnWhitespace) {
  std::vector<std::u16string> lines;

  // The parser requires a directory header before accepting regular input.
  lines.push_back(u"garbage");

  lines.push_back(u"   ");

  std::vector<FtpDirectoryListingEntry> entries;
  EXPECT_FALSE(ParseFtpDirectoryListingVms(lines, &entries));
}

}  // namespace

}  // namespace net
