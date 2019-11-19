// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::LocalFileStreamReader;

namespace content {

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = base::size(kTestData) - 1;

void NeverCalled(int) {
  ADD_FAILURE();
}

void QuitLoop() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace

class LocalFileStreamReaderTest : public testing::Test {
 public:
  LocalFileStreamReaderTest() : file_thread_("TestFileThread") {}

  void SetUp() override {
    ASSERT_TRUE(file_thread_.Start());
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    base::WriteFile(test_path(), kTestData, kTestDataSize);
    base::File::Info info;
    ASSERT_TRUE(base::GetFileInfo(test_path(), &info));
    test_file_modification_time_ = info.last_modified;
  }

  void TearDown() override {
    // Give another chance for deleted streams to perform Close.
    base::RunLoop().RunUntilIdle();
    file_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  LocalFileStreamReader* CreateFileReader(
      const base::FilePath& path,
      int64_t initial_offset,
      const base::Time& expected_modification_time) {
    return new LocalFileStreamReader(file_task_runner(), path, initial_offset,
                                     expected_modification_time);
  }

  void TouchTestFile(base::TimeDelta delta) {
    base::Time new_modified_time = test_file_modification_time() + delta;
    ASSERT_TRUE(base::TouchFile(test_path(), test_file_modification_time(),
                                new_modified_time));
  }

  base::SingleThreadTaskRunner* file_task_runner() const {
    return file_thread_.task_runner().get();
  }

  base::FilePath test_dir() const { return dir_.GetPath(); }
  base::FilePath test_path() const {
    return dir_.GetPath().AppendASCII("test");
  }
  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void EnsureFileTaskFinished() {
    file_task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         base::BindOnce(&QuitLoop));
    base::RunLoop().Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::Thread file_thread_;
  base::ScopedTempDir dir_;
  base::Time test_file_modification_time_;
};

TEST_F(LocalFileStreamReaderTest, NonExistent) {
  base::FilePath nonexistent_path = test_dir().AppendASCII("nonexistent");
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(nonexistent_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(LocalFileStreamReaderTest, Empty) {
  base::FilePath empty_path = test_dir().AppendASCII("empty");
  base::File file(empty_path, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  file.Close();

  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(empty_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  net::TestInt64CompletionCallback callback;
  int64_t length_result = reader->GetLength(callback.callback());
  if (length_result == net::ERR_IO_PENDING)
    length_result = callback.WaitForResult();
  ASSERT_EQ(0, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthNormal) {
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int64_t result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect.
  TouchTestFile(base::TimeDelta::FromSeconds(-1));

  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback1;
  int64_t result = reader->GetLength(callback1.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback1.WaitForResult();
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);

  // With nullptr expected modification time this should work.
  reader.reset(CreateFileReader(test_path(), 0, base::Time()));
  net::TestInt64CompletionCallback callback2;
  result = reader->GetLength(callback2.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback2.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthWithOffset) {
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  net::TestInt64CompletionCallback callback;
  int64_t result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, ReadNormal) {
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(LocalFileStreamReaderTest, ReadAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect. Note that the resolution on some filesystems
  // is 1s so we can't test with deltas less than that.
  TouchTestFile(base::TimeDelta::FromSeconds(-1));
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  EXPECT_EQ(0U, data.size());

  // Due to precision loss converting int64_t->double->int64_t (e.g. through
  // Blink) the expected/actual time may vary by microseconds. With
  // modification time delta < 10us this should work.
  TouchTestFile(base::TimeDelta::FromMicroseconds(1));
  data.clear();
  reader.reset(CreateFileReader(test_path(), 0, test_file_modification_time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);

  // With matching modification times time this should work.
  TouchTestFile(base::TimeDelta());
  data.clear();
  reader.reset(CreateFileReader(test_path(), 0, test_file_modification_time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);

  // And with nullptr expected modification time this should work.
  data.clear();
  reader.reset(CreateFileReader(test_path(), 0, base::Time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ(kTestData, data);
}

TEST_F(LocalFileStreamReaderTest, ReadWithOffset) {
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(LocalFileStreamReaderTest, DeleteWithUnfinishedRead) {
  std::unique_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kTestDataSize);
  int rv = reader->Read(buf.get(), buf->size(), base::BindOnce(&NeverCalled));
  ASSERT_TRUE(rv == net::ERR_IO_PENDING || rv >= 0);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader.reset();
  EnsureFileTaskFinished();
}

}  // namespace content
