// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_TEST_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_TEST_H_

#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

// An interface for derived FileStreamReader to implement.
// This allows multiple FileStreamReader implementations can share the
// same test framework.  Tests should implement CreateFileReader, WriteFile, and
// TouchFile to manipulate files for their particular implementation.
class FileStreamReaderTest : public testing::Test {
 public:
  static constexpr std::string_view kTestFileName = "test.dat";
  static constexpr std::string_view kTestData = "0123456789";

  virtual std::unique_ptr<FileStreamReader> CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time) = 0;
  virtual void WriteFile(const std::string& file_name,
                         std::string_view buffer,
                         base::Time* modification_time) = 0;
  // Adjust a file's last modified time by |delta|.
  virtual void TouchFile(const std::string& file_name,
                         base::TimeDelta delta) = 0;
  virtual void EnsureFileTaskFinished() {}

  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void WriteTestFile() {
    WriteFile(std::string(kTestFileName), kTestData,
              &test_file_modification_time_);
  }

  static void NeverCalled(int unused) { ADD_FAILURE(); }

 protected:
  // Must be listed before base::test::TaskEnvironment.
  base::ScopedTempDir dir_;

  // FileSystemContext queries QuotaDatabase, and even with MockQuotaManager
  // (which really fakes parts of QuotaManagerImpl), a thread pool is created
  // that requires TaskEnvironment.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::Time test_file_modification_time_;
};

template <class SubClass>
class FileStreamReaderTypedTest : public SubClass {
 public:
  void SetUp() override {
    SubClass::SetUp();
    this->WriteTestFile();
  }
};

TYPED_TEST_SUITE_P(FileStreamReaderTypedTest);

TYPED_TEST_P(FileStreamReaderTypedTest, NonExistent) {
  const char kFileName[] = "nonexistent";
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(kFileName, 0, base::Time()));
  auto data_or_error = ReadFromReader(*reader, /*bytes_to_read=*/10);
  ASSERT_FALSE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.error(), net::ERR_FILE_NOT_FOUND);
}

TYPED_TEST_P(FileStreamReaderTypedTest, Empty) {
  const char kFileName[] = "empty";
  this->WriteFile(kFileName, /*data=*/std::string_view(),
                  /*modification_time=*/nullptr);

  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(kFileName, 0, base::Time()));
  auto data_or_error = ReadFromReader(*reader, /*bytes_to_read=*/10);
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_THAT(data_or_error.value(), testing::IsEmpty());

  auto length_result = GetLengthFromReader(reader.get());
  ASSERT_TRUE(length_result.has_value());
  ASSERT_EQ(0, length_result.value());
}

TYPED_TEST_P(FileStreamReaderTypedTest, GetLengthNormal) {
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto result = GetLengthFromReader(reader.get());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(static_cast<int64_t>(this->kTestData.size()), result.value());
}

TYPED_TEST_P(FileStreamReaderTypedTest, GetLengthAfterModified) {
  this->TouchFile(std::string(this->kTestFileName), base::Seconds(10));

  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto result = GetLengthFromReader(reader.get());
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result.error());

  // With nullptr expected modification time this should work.
  reader =
      this->CreateFileReader(std::string(this->kTestFileName), 0, base::Time());
  result = GetLengthFromReader(reader.get());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(static_cast<int64_t>(this->kTestData.size()), result.value());
}

TYPED_TEST_P(FileStreamReaderTypedTest, GetLengthWithOffset) {
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 3, base::Time()));
  auto result = GetLengthFromReader(reader.get());
  // Initial offset does not affect the result of GetLength.
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(static_cast<int64_t>(this->kTestData.size()), result.value());
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadNormal) {
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect. Note that the resolution on some filesystems
  // is 1s so we can't test with deltas less than that.
  this->TouchFile(std::string(this->kTestFileName), base::Seconds(-1));

  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_FALSE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.error(), net::ERR_UPLOAD_FILE_CHANGED);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadAfterModifiedLessThanThreshold) {
  // Due to precision loss converting int64_t->double->int64_t (e.g. through
  // Blink) the expected/actual time may vary by microseconds. With
  // modification time delta < 10us this should work.
  this->TouchFile(std::string(this->kTestFileName), base::Microseconds(1));
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadAfterModifiedWithMatchingTimes) {
  this->TouchFile(std::string(this->kTestFileName), base::TimeDelta());
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadAfterModifiedWithoutExpectedTime) {
  this->TouchFile(std::string(this->kTestFileName), base::Seconds(-1));
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 0, base::Time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadWithOffset) {
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 3, base::Time()));
  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData.substr(3));
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadWithNegativeOffset) {
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), -1, base::Time()));
  auto data_or_error = ReadFromReader(*reader, /*bytes_to_read=*/1);
  ASSERT_FALSE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.error(), net::ERR_INVALID_ARGUMENT);
}

TYPED_TEST_P(FileStreamReaderTypedTest, ReadWithOffsetLargerThanFile) {
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName),
                             this->kTestData.size() + 1, base::Time()));
  auto data_or_error = ReadFromReader(*reader, /*bytes_to_read=*/1);
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_THAT(data_or_error.value(), testing::IsEmpty());
}

TYPED_TEST_P(FileStreamReaderTypedTest, DeleteWithUnfinishedRead) {
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(this->kTestData.size());
  int rv = reader->Read(buf.get(), buf->size(),
                        base::BindOnce(&FileStreamReaderTest::NeverCalled));
  if (rv < 0)
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader = nullptr;
  this->EnsureFileTaskFinished();
}

REGISTER_TYPED_TEST_SUITE_P(FileStreamReaderTypedTest,
                            NonExistent,
                            Empty,
                            GetLengthNormal,
                            GetLengthAfterModified,
                            GetLengthWithOffset,
                            ReadNormal,
                            ReadAfterModified,
                            ReadAfterModifiedLessThanThreshold,
                            ReadAfterModifiedWithMatchingTimes,
                            ReadAfterModifiedWithoutExpectedTime,
                            ReadWithOffset,
                            ReadWithNegativeOffset,
                            ReadWithOffsetLargerThanFile,
                            DeleteWithUnfinishedRead);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_TEST_H_
