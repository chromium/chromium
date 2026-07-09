// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/module/directory_listing.h"

#include <optional>

#include "base/byte_size.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* const raw_bytes;
  bool is_dir;
  std::optional<base::ByteSize> filesize;
  base::Time time;
  const char* const expected;
};

TEST(DirectoryListingTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
      {L"Subdir", "", false, std::nullopt, base::Time(),
       "<script>addRow(\"Subdir\",\"Subdir\",0,-1,\"\",0,\"\");</script>\n"},
      {L"Foo", "", false, base::ByteSize(10000), base::Time(),
       "<script>addRow(\"Foo\",\"Foo\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      {L"quo\"tes", "", false, base::ByteSize(10000), base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      {L"quo\"tes", "quo\"tes", false, base::ByteSize(10000), base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
      // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
      {L"\xD55C\xAE00.txt", "", false, base::ByteSize(10000), base::Time(),
       "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\","
       "\"%ED%95%9C%EA%B8%80.txt\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
      // a local or remote file in EUC-KR.
      {L"\xD55C\xAE00.txt", "\xC7\xD1\xB1\xDB.txt", false,
       base::ByteSize(10000), base::Time(),
       "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\",\"%C7%D1%B1%DB.txt\""
       ",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
  };

  for (const auto& test_case : test_cases) {
    const std::string results = GetDirectoryListingEntry(
        base::WideToUTF16(test_case.name), test_case.raw_bytes,
        test_case.is_dir, test_case.filesize, test_case.time);
    EXPECT_EQ(test_case.expected, results);
  }
}

TEST(DirectoryListingTest, GetSizeStringForTesting) {
  static const struct {
    base::ByteSize bytes;
    const char* expected;
  } cases[] = {
      // Expected behavior: we show one post-decimal digit when we have
      // under two pre-decimal digits, except in cases where it makes no
      // sense (zero or bytes).
      //
      // Since we switch units once we cross the 1000 mark, this keeps
      // the display of file sizes or bytes consistently around three
      // digits.
      {base::ByteSize(0), "0 B"},
      {base::ByteSize(512), "512 B"},
      {base::MiBU(1), "1.0 MB"},
      {base::GiBU(1), "1.0 GB"},
      {base::GiBU(10), "10.0 GB"},
      {base::GiBU(99), "99.0 GB"},
      {base::GiBU(105), "105 GB"},
      {base::GiBU(105) + base::MiBU(500), "105 GB"},
      {base::ByteSize::Max(), "8192 PB"},

      {base::KiBU(99) + base::ByteSize(103), "99.1 kB"},
      {base::MiBU(1) + base::ByteSize(103), "1.0 MB"},
      {base::MiBU(1) + base::KiBU(205), "1.2 MB"},
      {base::GiBU(1) + base::MiBU(927), "1.9 GB"},
      {base::GiBU(10), "10.0 GB"},
      {base::GiBU(100), "100 GB"},
  };

  for (const auto& i : cases) {
    EXPECT_EQ(i.expected, GetSizeStringForTesting(i.bytes));
  }
}

}  // namespace

}  // namespace net
