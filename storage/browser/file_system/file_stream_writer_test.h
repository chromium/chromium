// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_TEST_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_TEST_H_

#include <cstdio>
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

// An interface for derived FileStreamWriter to implement. This allows multiple
// FileStreamWriter implementations can share the same test framework. Tests
// should implement CreateFileWithContent, CreateWriter, FilePathExists, and
// GetFileContent to manipulate files for their particular implementation.
class FileStreamWriterTest : public testing::Test {
 public:
  static constexpr std::string_view kTestFileName = "file_a";

  virtual bool CreateFileWithContent(const std::string& name,
                                     const std::string& data) = 0;
  virtual std::unique_ptr<FileStreamWriter> CreateWriter(
      const std::string& name,
      int64_t offset) = 0;
  virtual bool FilePathExists(const std::string& name) = 0;
  virtual std::string GetFileContent(const std::string& name) = 0;

  static void NeverCalled(int unused) { ADD_FAILURE(); }

 protected:
  // Must be listed before base::test::TaskEnvironment.
  base::ScopedTempDir dir_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

template <class SubClass>
class FileStreamWriterTypedTest : public SubClass {
 public:
  void SetUp() override { SubClass::SetUp(); }
};

TYPED_TEST_SUITE_P(FileStreamWriterTypedTest);

TYPED_TEST_P(FileStreamWriterTypedTest, Write) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "bar"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foobar", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, WriteMiddle) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 2));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foxxxr", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, WriteNearEnd) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 5));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foobaxxx", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, WriteEnd) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 6));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foobarxxx",
            this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, WriteAfterEnd) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 7));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));

  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ(std::string("foobar\0xxx", 10),
            this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, WriteFailForNonexistingFile) {
  ASSERT_FALSE(this->FilePathExists(std::string(this->kTestFileName)));

  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, WriteStringToWriter(writer.get(), "foo"));

  EXPECT_FALSE(this->FilePathExists(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, CancelBeforeOperation) {
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));
  // Cancel immediately fails when there's no in-flight operation.
  EXPECT_EQ(net::ERR_UNEXPECTED, writer->Cancel(base::DoNothing()));
}

TYPED_TEST_P(FileStreamWriterTypedTest, CancelAfterFinishedOperation) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  // Cancel immediately fails when there's no in-flight operation.
  EXPECT_EQ(net::ERR_UNEXPECTED, writer->Cancel(base::DoNothing()));

  // Write operation is already completed.
  EXPECT_TRUE(this->FilePathExists(std::string(this->kTestFileName)));
  EXPECT_EQ("foobar", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, CancelWrite) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), "foobar"));
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));

  scoped_refptr<net::StringIOBuffer> buffer(
      base::MakeRefCounted<net::StringIOBuffer>("xxx"));
  int result =
      writer->Write(buffer.get(), buffer->size(),
                    base::BindOnce(&FileStreamWriterTest::NeverCalled));
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  net::TestCompletionCallback callback;
  writer->Cancel(callback.callback());
  int cancel_result = writer->Cancel(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(cancel_result));
}

TYPED_TEST_P(FileStreamWriterTypedTest, CancelFlush) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), ""));
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));

  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  int cancel_expectation = net::OK;
  int result = writer->Flush(FlushMode::kEndOfFile, base::DoNothing());
  // Flush can run synchronously or asynchronously.
  if (result == net::OK) {
    // Cancel() should error if called when there is no in-flight operation.
    cancel_expectation = net::ERR_UNEXPECTED;
  } else {
    EXPECT_EQ(net::ERR_IO_PENDING, result);
  }
  net::TestCompletionCallback callback;
  int cancel_result = writer->Cancel(callback.callback());
  EXPECT_EQ(cancel_expectation, callback.GetResult(cancel_result));

  EXPECT_EQ("foo", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, FlushBeforeWriting) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), ""));
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));

  EXPECT_EQ(net::OK, writer->Flush(FlushMode::kDefault, base::DoNothing()));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ("foo", this->GetFileContent(std::string(this->kTestFileName)));
}

TYPED_TEST_P(FileStreamWriterTypedTest, FlushAfterWriting) {
  EXPECT_TRUE(
      this->CreateFileWithContent(std::string(this->kTestFileName), ""));
  std::unique_ptr<FileStreamWriter> writer(
      this->CreateWriter(std::string(this->kTestFileName), 0));

  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  net::TestCompletionCallback callback;
  int result =
      writer->Flush(FlushMode::kEndOfFile,
                    base::OnceCallback<void(int)>(callback.callback()));
  ASSERT_EQ(net::OK, callback.GetResult(result));

  EXPECT_EQ("foo", this->GetFileContent(std::string(this->kTestFileName)));
}

REGISTER_TYPED_TEST_SUITE_P(FileStreamWriterTypedTest,
                            Write,
                            WriteMiddle,
                            WriteNearEnd,
                            WriteEnd,
                            WriteAfterEnd,
                            WriteFailForNonexistingFile,
                            CancelBeforeOperation,
                            CancelAfterFinishedOperation,
                            CancelWrite,
                            CancelFlush,
                            FlushBeforeWriting,
                            FlushAfterWriting);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_TEST_H_
