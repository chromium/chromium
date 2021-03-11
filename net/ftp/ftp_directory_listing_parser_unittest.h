// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_

#include <stdint.h>

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class FtpDirectoryListingParserTest : public testing::Test {
 public:
  struct SingleLineTestData {
    const char* input;
    FtpDirectoryListingEntry::Type type;
    const char* filename;
    int64_t size;
    int year;
    int month;
    int day_of_month;
    int hour;
    int minute;
  };

 protected:
  FtpDirectoryListingParserTest() {}

  std::vector<std::u16string> GetSingleLineTestCase(const std::string& text) {
    std::vector<std::u16string> lines;
    lines.push_back(base::UTF8ToUTF16(text));
    return lines;
  }

  void VerifySingleLineTestCase(
      const SingleLineTestData& test_case,
      const std::vector<FtpDirectoryListingEntry>& entries) {
    ASSERT_FALSE(entries.empty());

    FtpDirectoryListingEntry entry = entries[0];
    EXPECT_EQ(test_case.type, entry.type);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.filename), entry.name);
    EXPECT_EQ(test_case.size, entry.size);

    base::Time::Exploded time_exploded;
    entry.last_modified.UTCExplode(&time_exploded);

    // Only test members displayed on the directory listing.
    EXPECT_EQ(test_case.year, time_exploded.year);
    EXPECT_EQ(test_case.month, time_exploded.month);
    EXPECT_EQ(test_case.day_of_month, time_exploded.day_of_month);
    EXPECT_EQ(test_case.hour, time_exploded.hour);
    EXPECT_EQ(test_case.minute, time_exploded.minute);

    EXPECT_EQ(1U, entries.size());
  }

  base::Time GetMockCurrentTime() {
    base::Time::Exploded mock_current_time_exploded = { 0 };
    mock_current_time_exploded.year = 1994;
    mock_current_time_exploded.month = 11;
    mock_current_time_exploded.day_of_month = 15;
    mock_current_time_exploded.hour = 12;
    mock_current_time_exploded.minute = 45;

    base::Time out_time;
    EXPECT_TRUE(
        base::Time::FromUTCExploded(mock_current_time_exploded, &out_time));
    return out_time;
  }
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_
