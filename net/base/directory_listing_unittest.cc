// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_listing.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* const raw_bytes;
  bool is_dir;
  int64_t filesize;
  base::Time time;
  const char* const expected;
};

TEST(DirectoryListingTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
      {L"Foo", "", false, 10000, base::Time(),
       "<script>addRow(\"Foo\",\"Foo\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      {L"quo\"tes", "", false, 10000, base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      {L"quo\"tes", "quo\"tes", false, 10000, base::Time(),
       "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,10000,\"9.8 kB\",0,\"\""
       ");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
      // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
      {L"\xD55C\xAE00.txt", "", false, 10000, base::Time(),
       "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\","
       "\"%ED%95%9C%EA%B8%80.txt\",0,10000,\"9.8 kB\",0,\"\");</script>\n"},
      // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
      // a local or remote file in EUC-KR.
      {L"\xD55C\xAE00.txt", "\xC7\xD1\xB1\xDB.txt", false, 10000, base::Time(),
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

}  // namespace

}  // namespace net
