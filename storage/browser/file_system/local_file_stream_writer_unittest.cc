// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_writer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileStreamWriter;
using storage::LocalFileStreamWriter;

namespace content {

class LocalFileStreamWriterTest : public testing::Test {
 public:
  LocalFileStreamWriterTest() : file_thread_("TestFileThread") {}

  void SetUp() override {
    ASSERT_TRUE(file_thread_.Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    // Give another chance for deleted streams to perform Close.
    base::RunLoop().RunUntilIdle();
    file_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::FilePath Path(const std::string& name) {
    return temp_dir_.GetPath().AppendASCII(name);
  }

  std::string GetFileContent(const base::FilePath& path) {
    std::string content;
    base::ReadFileToString(path, &content);
    return content;
  }

  base::FilePath CreateFileWithContent(const std::string& name,
                                       const std::string& data) {
    base::FilePath path = Path(name);
    base::WriteFile(path, data.c_str(), data.size());
    return path;
  }

  base::SingleThreadTaskRunner* file_task_runner() const {
    return file_thread_.task_runner().get();
  }

  LocalFileStreamWriter* CreateWriter(const base::FilePath& path,
                                      int64_t offset) {
    return new LocalFileStreamWriter(file_task_runner(), path, offset,
                                     FileStreamWriter::OPEN_EXISTING_FILE);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::Thread file_thread_;
  base::ScopedTempDir temp_dir_;
};

void NeverCalled(int unused) {
  ADD_FAILURE();
}

TEST_F(LocalFileStreamWriterTest, Write) {
  base::FilePath path = CreateFileWithContent("file_a", std::string());
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "bar"));
  writer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_EQ("foobar", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteMiddle) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 2));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  writer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_EQ("foxxxr", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteEnd) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 6));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  writer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_EQ("foobarxxx", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteFailForNonexistingFile) {
  base::FilePath path = Path("file_a");
  ASSERT_FALSE(base::PathExists(path));
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, WriteStringToWriter(writer.get(), "foo"));
  writer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(LocalFileStreamWriterTest, CancelBeforeOperation) {
  base::FilePath path = Path("file_a");
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 0));
  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer->Cancel(base::BindOnce(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);
}

TEST_F(LocalFileStreamWriterTest, CancelAfterFinishedOperation) {
  base::FilePath path = CreateFileWithContent("file_a", std::string());
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer->Cancel(base::BindOnce(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);

  writer.reset();
  base::RunLoop().RunUntilIdle();
  // Write operation is already completed.
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_EQ("foo", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, CancelWrite) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  std::unique_ptr<LocalFileStreamWriter> writer(CreateWriter(path, 0));

  scoped_refptr<net::StringIOBuffer> buffer(
      base::MakeRefCounted<net::StringIOBuffer>("xxx"));
  int result =
      writer->Write(buffer.get(), buffer->size(), base::BindOnce(&NeverCalled));
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  net::TestCompletionCallback callback;
  writer->Cancel(callback.callback());
  int cancel_result = callback.WaitForResult();
  EXPECT_EQ(net::OK, cancel_result);
}

}  // namespace content
