// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using storage::FileSystemContext;
using storage::FileSystemFileStreamReader;
using storage::FileSystemType;
using storage::FileSystemURL;

namespace content {

namespace {

const char kURLOrigin[] = "http://remote/";
const char kTestFileName[] = "test.dat";
const char kTestData[] = "0123456789";
const int kTestDataSize = base::size(kTestData) - 1;

void NeverCalled(int unused) {
  ADD_FAILURE();
}

}  // namespace

class FileSystemFileStreamReaderTest : public testing::Test {
 public:
  FileSystemFileStreamReaderTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ =
        CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

    file_system_context_->OpenFileSystem(
        GURL(kURLOrigin), storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&OnOpenFileSystem));
    base::RunLoop().RunUntilIdle();

    WriteFile(kTestFileName, kTestData, kTestDataSize,
              &test_file_modification_time_);
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  storage::FileSystemFileStreamReader* CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time) {
    return new FileSystemFileStreamReader(
        file_system_context_.get(), GetFileSystemURL(file_name), initial_offset,
        expected_modification_time);
  }

  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void WriteFile(const std::string& file_name,
                 const char* buf,
                 int buf_size,
                 base::Time* modification_time) {
    FileSystemURL url = GetFileSystemURL(file_name);

    ASSERT_EQ(base::File::FILE_OK,
              content::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url, buf, buf_size));

    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetMetadata(file_system_context_.get(), url,
                                               &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

 private:
  static void OnOpenFileSystem(const GURL& root_url,
                               const std::string& name,
                               base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
  }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL(kURLOrigin), storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::Time test_file_modification_time_;
};

TEST_F(FileSystemFileStreamReaderTest, NonExistent) {
  const char kFileName[] = "nonexistent";
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(FileSystemFileStreamReaderTest, Empty) {
  const char kFileName[] = "empty";
  WriteFile(kFileName, nullptr, 0, nullptr);

  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  net::TestInt64CompletionCallback callback;
  int64_t length_result = reader->GetLength(callback.callback());
  if (length_result == net::ERR_IO_PENDING)
    length_result = callback.WaitForResult();
  ASSERT_EQ(0, length_result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthNormal) {
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int64_t result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  net::TestInt64CompletionCallback callback1;
  int64_t result = reader->GetLength(callback1.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback1.WaitForResult();
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);

  // With nullptr expected modification time this should work.
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  net::TestInt64CompletionCallback callback2;
  result = reader->GetLength(callback2.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback2.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthWithOffset) {
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  net::TestInt64CompletionCallback callback;
  int64_t result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, ReadNormal) {
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  ASSERT_EQ(0U, data.size());

  // With nullptr expected modification time this should work.
  data.clear();
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadWithOffset) {
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(FileSystemFileStreamReaderTest, DeleteWithUnfinishedRead) {
  std::unique_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kTestDataSize);
  int rv = reader->Read(buf.get(), buf->size(), base::BindOnce(&NeverCalled));
  ASSERT_TRUE(rv == net::ERR_IO_PENDING || rv >= 0);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader.reset();
}

}  // namespace content
