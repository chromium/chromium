// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/sandbox_database_test_helper.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using storage::FilePathToString;

namespace content {

void CorruptDatabase(const base::FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size) {
  base::FileEnumerator file_enum(db_path, false /* not recursive */,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  base::FilePath file_path;
  base::FilePath picked_file_path;
  uint64_t picked_file_number = std::numeric_limits<uint64_t>::max();

  while (!(file_path = file_enum.Next()).empty()) {
    uint64_t number = std::numeric_limits<uint64_t>::max();
    leveldb::FileType file_type;
    EXPECT_TRUE(leveldb_chrome::ParseFileName(
        FilePathToString(file_path.BaseName()), &number, &file_type));
    if (file_type == type &&
        (picked_file_number == std::numeric_limits<uint64_t>::max() ||
         picked_file_number < number)) {
      picked_file_path = file_path;
      picked_file_number = number;
    }
  }

  EXPECT_FALSE(picked_file_path.empty());
  EXPECT_NE(std::numeric_limits<uint64_t>::max(), picked_file_number);

  base::File file(picked_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ |
                      base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(file.created());

  base::File::Info file_info;
  EXPECT_TRUE(file.GetInfo(&file_info));
  if (offset < 0)
    offset += file_info.size;
  EXPECT_GE(offset, 0);
  EXPECT_LE(offset, file_info.size);

  size = std::min(size, static_cast<size_t>(file_info.size - offset));

  std::vector<char> buf(size);
  int read_size = file.Read(offset, buf.data(), buf.size());
  EXPECT_LT(0, read_size);
  EXPECT_GE(buf.size(), static_cast<size_t>(read_size));
  buf.resize(read_size);

  std::transform(buf.begin(), buf.end(), buf.begin(),
                 std::logical_not<char>());

  int written_size = file.Write(offset, buf.data(), buf.size());
  EXPECT_GT(written_size, 0);
  EXPECT_EQ(buf.size(), static_cast<size_t>(written_size));
}

void DeleteDatabaseFile(const base::FilePath& db_path,
                        leveldb::FileType type) {
  base::FileEnumerator file_enum(db_path, false /* not recursive */,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  base::FilePath file_path;
  while (!(file_path = file_enum.Next()).empty()) {
    uint64_t number = std::numeric_limits<uint64_t>::max();
    leveldb::FileType file_type;
    EXPECT_TRUE(leveldb_chrome::ParseFileName(
        FilePathToString(file_path.BaseName()), &number, &file_type));
    if (file_type == type) {
      base::DeleteFile(file_path, false);
      // We may have multiple files for the same type, so don't break here.
    }
  }
}

}  // namespace content
