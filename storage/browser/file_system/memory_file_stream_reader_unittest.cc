// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = base::size(kTestData) - 1;

}  // namespace

class MemoryFileStreamReaderTest : public testing::Test {
 public:
  MemoryFileStreamReaderTest() {}

  void SetUp() override {
    ASSERT_TRUE(file_system_directory_.CreateUniqueTempDir());
    file_util_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(
        file_system_directory_.GetPath());

    file_util_->CreateFileForTesting(
        test_path(), base::span<const char>(kTestData, kTestDataSize));
    base::File::Info info;
    ASSERT_EQ(base::File::FILE_OK, file_util_->GetFileInfo(test_path(), &info));
    test_file_modification_time_ = info.last_modified;
  }

  void TearDown() override {
    // In memory operations should not have any residue in file system
    // directory.
    EXPECT_TRUE(base::IsDirectoryEmpty(file_system_directory_.GetPath()));
  }

  ObfuscatedFileUtilMemoryDelegate* file_util() { return file_util_.get(); }

 protected:
  std::unique_ptr<FileStreamReader> CreateFileReader(
      const base::FilePath& path,
      int64_t initial_offset,
      const base::Time& expected_modification_time) {
    return FileStreamReader::CreateForMemoryFile(file_util_->GetWeakPtr(), path,
                                                 initial_offset,
                                                 expected_modification_time);
  }

  void TouchTestFile(base::TimeDelta delta) {
    base::Time new_modified_time = test_file_modification_time() + delta;
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->Touch(test_path(), test_file_modification_time(),
                                 new_modified_time));
  }

  base::FilePath test_dir() const { return file_system_directory_.GetPath(); }
  base::FilePath test_path() const {
    return file_system_directory_.GetPath().AppendASCII("test");
  }
  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

 private:
  base::ScopedTempDir file_system_directory_;
  std::unique_ptr<ObfuscatedFileUtilMemoryDelegate> file_util_;
  base::Time test_file_modification_time_;
};

TEST_F(MemoryFileStreamReaderTest, NonExistent) {
  base::FilePath nonexistent_path = test_dir().AppendASCII("nonexistent");
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(nonexistent_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(MemoryFileStreamReaderTest, Empty) {
  base::FilePath empty_path = test_dir().AppendASCII("empty");
  bool created;
  EXPECT_EQ(base::File::FILE_OK,
            file_util()->EnsureFileExists(empty_path, &created));

  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(empty_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  int64_t length_result = reader->GetLength(base::DoNothing());
  ASSERT_EQ(0, length_result);
}

TEST_F(MemoryFileStreamReaderTest, GetLengthNormal) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int64_t result = reader->GetLength(base::DoNothing());
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(MemoryFileStreamReaderTest, GetLengthAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect.
  TouchTestFile(base::TimeDelta::FromSeconds(-1));

  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int64_t result = reader->GetLength(base::DoNothing());
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
}

TEST_F(MemoryFileStreamReaderTest, GetLengthAfterModifiedWithNoExpectedTime) {
  // Touch file so that the file's modification time becomes different
  // from what we expect.
  TouchTestFile(base::TimeDelta::FromSeconds(-1));

  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, base::Time()));
  int64_t result = reader->GetLength(base::DoNothing());
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(MemoryFileStreamReaderTest, GetLengthWithOffset) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  int64_t result = reader->GetLength(base::DoNothing());
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(MemoryFileStreamReaderTest, ReadNormal) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(MemoryFileStreamReaderTest, ReadAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect. Note that the resolution on some filesystems
  // is 1s so we can't test with deltas less than that.
  TouchTestFile(base::TimeDelta::FromSeconds(-1));
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  EXPECT_EQ(0U, data.size());
}

TEST_F(MemoryFileStreamReaderTest, ReadAfterModifiedLessThanThreshold) {
  // Due to precision loss converting int64_t->double->int64_t (e.g. through
  // Blink) the expected/actual time may vary by microseconds. With
  // modification time delta < 10us this should work.
  TouchTestFile(base::TimeDelta::FromMicroseconds(1));
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;

  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);
}

TEST_F(MemoryFileStreamReaderTest, ReadAfterModifiedWithMatchingTimes) {
  TouchTestFile(base::TimeDelta());
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;

  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);
}

TEST_F(MemoryFileStreamReaderTest, ReadAfterModifiedWithoutExpectedTime) {
  TouchTestFile(base::TimeDelta());
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 0, base::Time()));
  int result = 0;
  std::string data;

  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);
}

TEST_F(MemoryFileStreamReaderTest, ReadWithOffset) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(MemoryFileStreamReaderTest, ReadWithNegativeOffset) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), -1, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 1, &result);
  ASSERT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, result);
}

TEST_F(MemoryFileStreamReaderTest, ReadWithOffsetLargerThanFile) {
  std::unique_ptr<FileStreamReader> reader(
      CreateFileReader(test_path(), kTestDataSize + 1, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 1, &result);
  ASSERT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, result);
}

}  // namespace storage
