// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_listing.h"

#include "base/byte_count.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* const raw_bytes;
  bool is_dir;
  base::ByteCount filesize;
  base::Time time;
  const char* const expected;
};

TEST(DirectoryListingTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
      {L"Foo", "", false, base::ByteCount(10000), base::Time(),
       "<script>addRow(\"Foo\",\"Foo\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      {L"quo\"tes", "", false, base::ByteCount(10000), base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      {L"quo\"tes", "quo\"tes", false, base::ByteCount(10000), base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
      // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
      {L"\xD55C\xAE00.txt", "", false, base::ByteCount(10000), base::Time(),
       "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\","
       "\"%ED%95%9C%EA%B8%80.txt\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
      // a local or remote file in EUC-KR.
      {L"\xD55C\xAE00.txt", "\xC7\xD1\xB1\xDB.txt", false,
       base::ByteCount(10000), base::Time(),
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
    base::ByteCount bytes;
    const char* expected;
  } cases[] = {
      // Expected behavior: we show one post-decimal digit when we have
      // under two pre-decimal digits, except in cases where it makes no
      // sense (zero or bytes).
      //
      // Since we switch units once we cross the 1000 mark, this keeps
      // the display of file sizes or bytes consistently around three
      // digits.
      {base::ByteCount(0), "0 B"},
      {base::ByteCount(512), "512 B"},
      {base::MiB(1), "1.0 MB"},
      {base::GiB(1), "1.0 GB"},
      {base::GiB(10), "10.0 GB"},
      {base::GiB(99), "99.0 GB"},
      {base::GiB(105), "105 GB"},
      {base::GiB(105) + base::MiB(500), "105 GB"},
      {base::ByteCount::Max(), "8192 PB"},

      {base::KiB(99) + base::ByteCount(103), "99.1 kB"},
      {base::MiB(1) + base::ByteCount(103), "1.0 MB"},
      {base::MiB(1) + base::KiB(205), "1.2 MB"},
      {base::GiB(1) + base::MiB(927), "1.9 GB"},
      {base::GiB(10), "10.0 GB"},
      {base::GiB(100), "100 GB"},
  };

  for (const auto& i : cases) {
    EXPECT_EQ(i.expected, GetSizeStringForTesting(i.bytes));
  }
}

}  // namespace

}  // namespace net
