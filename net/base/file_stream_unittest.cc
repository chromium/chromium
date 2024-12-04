// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/file_stream.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/test_net_log.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

#if BUILDFLAG(IS_ANDROID)
#include "base/test/test_file_util.h"
#endif

namespace net {

namespace {

constexpr char kTestData[] = "0123456789";
constexpr int kTestDataSize = std::size(kTestData) - 1;

// Creates an IOBufferWithSize that contains the kTestDataSize.
scoped_refptr<IOBufferWithSize> CreateTestDataBuffer() {
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kTestDataSize);
  memcpy(buf->data(), kTestData, kTestDataSize);
  return buf;
}

}  // namespace

class FileStreamTest : public PlatformTest, public WithTaskEnvironment {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    base::CreateTemporaryFile(&temp_file_path_);
    base::WriteFile(temp_file_path_, kTestData);
  }
  void TearDown() override {
    // FileStreamContexts must be asynchronously closed on the file task runner
    // before they can be deleted. Pump the RunLoop to avoid leaks.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(base::DeleteFile(temp_file_path_));

    PlatformTest::TearDown();
  }

  const base::FilePath temp_file_path() const { return temp_file_path_; }

 private:
  base::FilePath temp_file_path_;
};

namespace {

TEST_F(FileStreamTest, OpenExplicitClose) {
  TestCompletionCallback callback;
  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(stream.IsOpen());
  EXPECT_THAT(stream.Close(callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_FALSE(stream.IsOpen());
}

TEST_F(FileStreamTest, OpenExplicitCloseOrphaned) {
  TestCompletionCallback callback;
  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(stream->IsOpen());
  EXPECT_THAT(stream->Close(callback.callback()), IsError(ERR_IO_PENDING));
  stream.reset();
  // File isn't actually closed yet.
  base::RunLoop runloop;
  runloop.RunUntilIdle();
  // The file should now be closed, though the callback has not been called.
}

// Test the use of FileStream with a file handle provided at construction.
TEST_F(FileStreamTest, UseFileHandle) {
  int rv = 0;
  TestCompletionCallback callback;
  TestInt64CompletionCallback callback64;
  // 1. Test reading with a file handle.
  ASSERT_TRUE(base::WriteFile(temp_file_path(), kTestData));
  int flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  base::File file1(temp_file_path(), flags);

  // Seek to the beginning of the file and read.
  auto read_stream = std::make_unique<FileStream>(
      std::move(file1), base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_THAT(read_stream->Seek(0, callback64.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(0, callback64.WaitForResult());
  // Read into buffer and compare.
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kTestDataSize);
  rv = read_stream->Read(read_buffer.get(), kTestDataSize, callback.callback());
  ASSERT_EQ(kTestDataSize, callback.GetResult(rv));
  ASSERT_EQ(0, memcmp(kTestData, read_buffer->data(), kTestDataSize));
  read_stream.reset();

  // 2. Test writing with a file handle.
  base::DeleteFile(temp_file_path());
  flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
          base::File::FLAG_ASYNC;
  base::File file2(temp_file_path(), flags);

  auto write_stream = std::make_unique<FileStream>(
      std::move(file2), base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_THAT(write_stream->Seek(0, callback64.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(0, callback64.WaitForResult());
  scoped_refptr<IOBufferWithSize> write_buffer = CreateTestDataBuffer();
  rv = write_stream->Write(write_buffer.get(), kTestDataSize,
                           callback.callback());
  ASSERT_EQ(kTestDataSize, callback.GetResult(rv));
  write_stream.reset();

  // Read into buffer and compare to make sure the handle worked fine.
  ASSERT_EQ(kTestDataSize,
            base::ReadFile(temp_file_path(), read_buffer->data(),
                           kTestDataSize));
  ASSERT_EQ(0, memcmp(kTestData, read_buffer->data(), kTestDataSize));
}

TEST_F(FileStreamTest, UseClosedStream) {
  int rv = 0;
  TestCompletionCallback callback;
  TestInt64CompletionCallback callback64;

  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());

  EXPECT_FALSE(stream.IsOpen());

  // Try seeking...
  rv = stream.Seek(5, callback64.callback());
  EXPECT_THAT(callback64.GetResult(rv), IsError(ERR_UNEXPECTED));

  // Try reading...
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = stream.Read(buf.get(), buf->size(), callback.callback());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_UNEXPECTED));
}

TEST_F(FileStreamTest, Read) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(4);
    rv = stream.Read(buf.get(), buf->size(), callback.callback());
    rv = callback.GetResult(rv);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size.value(), total_bytes_read);
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(FileStreamTest, Read_EarlyDelete) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(4);
  rv = stream->Read(buf.get(), buf->size(), callback.callback());
  stream.reset();  // Delete instead of closing it.
  if (rv < 0) {
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    // The callback should not be called if the request is cancelled.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(callback.have_result());
  } else {
    EXPECT_EQ(std::string(kTestData, rv), std::string(buf->data(), rv));
  }
}

TEST_F(FileStreamTest, Read_FromOffset) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  TestInt64CompletionCallback callback64;
  const int64_t kOffset = 3;
  rv = stream.Seek(kOffset, callback64.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  int64_t new_offset = callback64.WaitForResult();
  EXPECT_EQ(kOffset, new_offset);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(4);
    rv = stream.Read(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size.value() - kOffset, total_bytes_read);
  EXPECT_EQ(kTestData + kOffset, data_read);
}

TEST_F(FileStreamTest, Write) {
  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  EXPECT_THAT(file_size, testing::Optional(0));

  scoped_refptr<IOBuffer> buf = CreateTestDataBuffer();
  rv = stream.Write(buf.get(), kTestDataSize, callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(kTestDataSize, rv);

  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(kTestDataSize, file_size.value());

  std::string data_read;
  EXPECT_TRUE(base::ReadFileToString(temp_file_path(), &data_read));
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(FileStreamTest, Write_EarlyDelete) {
  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(0, file_size.value());

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  rv = stream->Write(buf.get(), buf->size(), callback.callback());
  stream.reset();
  if (rv < 0) {
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    // The callback should not be called if the request is cancelled.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(callback.have_result());
  } else {
    file_size = base::GetFileSize(temp_file_path());
    ASSERT_TRUE(file_size.has_value());
    EXPECT_EQ(file_size.value(), rv);
  }
}

TEST_F(FileStreamTest, Write_FromOffset) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_WRITE |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  TestInt64CompletionCallback callback64;
  const int64_t kOffset = kTestDataSize;
  rv = stream.Seek(kOffset, callback64.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  int64_t new_offset = callback64.WaitForResult();
  EXPECT_EQ(kTestDataSize, new_offset);

  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buffer = CreateTestDataBuffer();
  int buffer_size = buffer->size();
  scoped_refptr<DrainableIOBuffer> drainable =
      base::MakeRefCounted<DrainableIOBuffer>(std::move(buffer), buffer_size);
  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(drainable.get(), drainable->BytesRemaining(),
                      callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }
  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(file_size, kTestDataSize * 2);
}

TEST_F(FileStreamTest, BasicReadWrite) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  int64_t total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(4);
    rv = stream->Read(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);

  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buffer = CreateTestDataBuffer();
  int buffer_size = buffer->size();
  scoped_refptr<DrainableIOBuffer> drainable =
      base::MakeRefCounted<DrainableIOBuffer>(std::move(buffer), buffer_size);
  while (total_bytes_written != kTestDataSize) {
    rv = stream->Write(drainable.get(), drainable->BytesRemaining(),
                       callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }

  stream.reset();

  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, BasicWriteRead) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  TestInt64CompletionCallback callback64;
  rv = stream->Seek(file_size.value(), callback64.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  int64_t offset = callback64.WaitForResult();
  EXPECT_EQ(offset, file_size.value());

  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buffer = CreateTestDataBuffer();
  int buffer_size = buffer->size();
  scoped_refptr<DrainableIOBuffer> drainable =
      base::MakeRefCounted<DrainableIOBuffer>(std::move(buffer), buffer_size);
  while (total_bytes_written != kTestDataSize) {
    rv = stream->Write(drainable.get(), drainable->BytesRemaining(),
                       callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }

  EXPECT_EQ(kTestDataSize, total_bytes_written);

  rv = stream->Seek(0, callback64.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  offset = callback64.WaitForResult();
  EXPECT_EQ(0, offset);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(4);
    rv = stream->Read(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  stream.reset();

  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(kTestDataSize * 2, file_size.value());

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteReadCompletionCallback {
 public:
  TestWriteReadCompletionCallback(FileStream* stream,
                                  int* total_bytes_written,
                                  int* total_bytes_read,
                                  std::string* data_read)
      : stream_(stream),
        total_bytes_written_(total_bytes_written),
        total_bytes_read_(total_bytes_read),
        data_read_(data_read),
        drainable_(
            base::MakeRefCounted<DrainableIOBuffer>(CreateTestDataBuffer(),
                                                    kTestDataSize)) {}

  TestWriteReadCompletionCallback(const TestWriteReadCompletionCallback&) =
      delete;
  TestWriteReadCompletionCallback& operator=(
      const TestWriteReadCompletionCallback&) = delete;

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      base::RunLoop loop;
      quit_closure_ = loop.QuitWhenIdleClosure();
      waiting_for_result_ = true;
      loop.Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

  CompletionOnceCallback callback() {
    return base::BindOnce(&TestWriteReadCompletionCallback::OnComplete,
                          base::Unretained(this));
  }

  void ValidateWrittenData() {
    TestCompletionCallback callback;
    int rv = 0;
    for (;;) {
      scoped_refptr<IOBufferWithSize> buf =
          base::MakeRefCounted<IOBufferWithSize>(4);
      rv = stream_->Read(buf.get(), buf->size(), callback.callback());
      if (rv == ERR_IO_PENDING) {
        rv = callback.WaitForResult();
      }
      EXPECT_LE(0, rv);
      if (rv <= 0)
        break;
      *total_bytes_read_ += rv;
      data_read_->append(buf->data(), rv);
    }
  }

 private:
  void OnComplete(int result) {
    DCHECK_LT(0, result);
    *total_bytes_written_ += result;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0, total_bytes_read = 0;
      std::string data_read;
      TestWriteReadCompletionCallback callback(
          stream_, &total_bytes_written, &total_bytes_read, &data_read);
      rv = stream_->Write(
          drainable_.get(), drainable_->BytesRemaining(), callback.callback());
      DCHECK_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      drainable_->DidConsume(total_bytes_written);
      *total_bytes_written_ += total_bytes_written;
      *total_bytes_read_ += total_bytes_read;
      *data_read_ += data_read;
    } else {  // We're done writing all data.  Start reading the data.
      TestInt64CompletionCallback callback64;
      EXPECT_THAT(stream_->Seek(0, callback64.callback()),
                  IsError(ERR_IO_PENDING));
      {
        EXPECT_LE(0, callback64.WaitForResult());
      }
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      std::move(quit_closure_).Run();
  }

  int result_ = 0;
  bool have_result_ = false;
  bool waiting_for_result_ = false;
  raw_ptr<FileStream> stream_;
  raw_ptr<int> total_bytes_written_;
  raw_ptr<int> total_bytes_read_;
  raw_ptr<std::string> data_read_;
  scoped_refptr<DrainableIOBuffer> drainable_;
  base::OnceClosure quit_closure_;
};

TEST_F(FileStreamTest, WriteRead) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream->Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(open_callback.WaitForResult(), IsOk());

  TestInt64CompletionCallback callback64;
  EXPECT_THAT(stream->Seek(file_size.value(), callback64.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_EQ(file_size.value(), callback64.WaitForResult());

  int total_bytes_written = 0;
  int total_bytes_read = 0;
  std::string data_read;
  {
    // `callback` can't outlive `stream`.
    TestWriteReadCompletionCallback callback(stream.get(), &total_bytes_written,
                                             &total_bytes_read, &data_read);

    scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
    rv = stream->Write(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    EXPECT_LT(0, rv);
    EXPECT_EQ(kTestDataSize, total_bytes_written);

    callback.ValidateWrittenData();
  }
  stream.reset();

  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(kTestDataSize * 2, file_size.value());

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteCloseCompletionCallback {
 public:
  TestWriteCloseCompletionCallback(FileStream* stream, int* total_bytes_written)
      : stream_(stream),
        total_bytes_written_(total_bytes_written),
        drainable_(
            base::MakeRefCounted<DrainableIOBuffer>(CreateTestDataBuffer(),
                                                    kTestDataSize)) {}
  TestWriteCloseCompletionCallback(const TestWriteCloseCompletionCallback&) =
      delete;
  TestWriteCloseCompletionCallback& operator=(
      const TestWriteCloseCompletionCallback&) = delete;

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      base::RunLoop loop;
      quit_closure_ = loop.QuitWhenIdleClosure();
      waiting_for_result_ = true;
      loop.Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

  CompletionOnceCallback callback() {
    return base::BindOnce(&TestWriteCloseCompletionCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    DCHECK_LT(0, result);
    *total_bytes_written_ += result;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0;
      TestWriteCloseCompletionCallback callback(stream_, &total_bytes_written);
      rv = stream_->Write(
          drainable_.get(), drainable_->BytesRemaining(), callback.callback());
      DCHECK_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      drainable_->DidConsume(total_bytes_written);
      *total_bytes_written_ += total_bytes_written;
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      std::move(quit_closure_).Run();
  }

  int result_ = 0;
  bool have_result_ = false;
  bool waiting_for_result_ = false;
  raw_ptr<FileStream> stream_;
  raw_ptr<int> total_bytes_written_;
  scoped_refptr<DrainableIOBuffer> drainable_;
  base::OnceClosure quit_closure_;
};

TEST_F(FileStreamTest, WriteClose) {
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());

  auto stream = std::make_unique<FileStream>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream->Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(open_callback.WaitForResult(), IsOk());

  TestInt64CompletionCallback callback64;
  EXPECT_THAT(stream->Seek(file_size.value(), callback64.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_EQ(file_size, callback64.WaitForResult());

  int total_bytes_written = 0;
  {
    // `callback` can't outlive `stream`.
    TestWriteCloseCompletionCallback callback(stream.get(),
                                              &total_bytes_written);
    scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
    rv = stream->Write(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING) {
      total_bytes_written = callback.WaitForResult();
    }
    EXPECT_LT(0, total_bytes_written);
    EXPECT_EQ(kTestDataSize, total_bytes_written);
  }
  stream.reset();

  file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(kTestDataSize * 2, file_size.value());
}

TEST_F(FileStreamTest, OpenAndDelete) {
  base::Thread worker_thread("StreamTest");
  ASSERT_TRUE(worker_thread.Start());

  base::ScopedDisallowBlocking disallow_blocking;
  auto stream = std::make_unique<FileStream>(worker_thread.task_runner());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_WRITE |
              base::File::FLAG_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream->Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Delete the stream without waiting for the open operation to be
  // complete. Should be safe.
  stream.reset();

  // Force an operation through the worker.
  auto stream2 = std::make_unique<FileStream>(worker_thread.task_runner());
  TestCompletionCallback open_callback2;
  rv = stream2->Open(temp_file_path(), flags, open_callback2.callback());
  EXPECT_THAT(open_callback2.GetResult(rv), IsOk());
  stream2.reset();

  // open_callback won't be called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(open_callback.have_result());
}

// Verify that Write() errors are mapped correctly.
TEST_F(FileStreamTest, WriteError) {
  // Try opening file as read-only and then writing to it using FileStream.
  uint32_t flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;

  base::File file(temp_file_path(), flags);
  ASSERT_TRUE(file.IsValid());

  auto stream = std::make_unique<FileStream>(
      std::move(file), base::SingleThreadTaskRunner::GetCurrentDefault());

  scoped_refptr<IOBuffer> buf = base::MakeRefCounted<IOBufferWithSize>(1);
  buf->data()[0] = 0;

  TestCompletionCallback callback;
  int rv = stream->Write(buf.get(), 1, callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_LT(rv, 0);

  stream.reset();
  base::RunLoop().RunUntilIdle();
}

// Verify that Read() errors are mapped correctly.
TEST_F(FileStreamTest, ReadError) {
  // Try opening file for write and then reading from it using FileStream.
  uint32_t flags =
      base::File::FLAG_OPEN | base::File::FLAG_WRITE | base::File::FLAG_ASYNC;

  base::File file(temp_file_path(), flags);
  ASSERT_TRUE(file.IsValid());

  auto stream = std::make_unique<FileStream>(
      std::move(file), base::SingleThreadTaskRunner::GetCurrentDefault());

  scoped_refptr<IOBuffer> buf = base::MakeRefCounted<IOBufferWithSize>(1);
  TestCompletionCallback callback;
  int rv = stream->Read(buf.get(), 1, callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_LT(rv, 0);

  stream.reset();
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_WIN)
// Verifies that a FileStream will close itself if it receives a File whose
// async flag doesn't match the async state of the underlying handle.
TEST_F(FileStreamTest, AsyncFlagMismatch) {
  // Open the test file without async, then make a File with the same sync
  // handle but with the async flag set to true.
  uint32_t flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File file(temp_file_path(), flags);
  base::File lying_file(file.TakePlatformFile(), true);
  ASSERT_TRUE(lying_file.IsValid());

  FileStream stream(std::move(lying_file),
                    base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_FALSE(stream.IsOpen());
  TestCompletionCallback callback;
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(4);
  int rv = stream.Read(buf.get(), buf->size(), callback.callback());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_UNEXPECTED));
}
#endif

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/41420277): flaky on both android and cronet bots.
TEST_F(FileStreamTest, DISABLED_ContentUriRead) {
  base::FilePath test_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_dir);
  test_dir = test_dir.AppendASCII("net");
  test_dir = test_dir.AppendASCII("data");
  test_dir = test_dir.AppendASCII("file_stream_unittest");
  ASSERT_TRUE(base::PathExists(test_dir));
  base::FilePath image_file = test_dir.Append(FILE_PATH_LITERAL("red.png"));

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  base::FilePath path = base::InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(base::PathExists(path));
  std::optional<int64_t> file_size = base::GetFileSize(temp_file_path());
  ASSERT_TRUE(file_size.has_value());
  EXPECT_LT(0, file_size.value());

  FileStream stream(base::SingleThreadTaskRunner::GetCurrentDefault());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(path, flags, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(4);
    rv = stream.Read(buf.get(), buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size.value(), total_bytes_read);
}
#endif

#if BUILDFLAG(IS_WIN)
// A test fixture with helpers to create and connect to a named pipe for the
// sake of testing FileStream::ConnectNamedPipe().
class FileStreamPipeTest : public PlatformTest, public WithTaskEnvironment {
 protected:
  FileStreamPipeTest() = default;

  // Creates a named pipe (of name `pipe_name_`) for asynchronous use. Returns a
  // `File` wrapping it or an error.
  base::File CreatePipe() {
    base::win::ScopedHandle pipe(::CreateNamedPipeW(
        pipe_name_.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE |
            FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE, /*nMaxInstances=*/1,
        /*nOutBufferSize=*/0, /*nInBufferSize=*/0, /*nDefaultTimeOut=*/0,
        /*lpSecurityAttributes=*/nullptr));
    if (pipe.IsValid()) {
      return base::File(std::move(pipe), /*async=*/true);
    }
    return base::File(base::File::GetLastFileError());
  }

  // Opens the pipe named `pipe_name_`, which must have previously been created
  // via CreatePipe(). Returns a `File` wrapping it or an error.
  base::File OpenPipe() {
    base::win::ScopedHandle pipe(
        ::CreateFileW(pipe_name_.c_str(), GENERIC_READ | GENERIC_WRITE,
                      /*dwShareMode=*/0, /*lpSecurityAttributes=*/nullptr,
                      OPEN_EXISTING, /*dwFlagsAndAttributes=*/0,
                      /*hTemplateFile=*/nullptr));
    if (!pipe.IsValid()) {
      return base::File(base::File::GetLastFileError());
    }
    return base::File(std::move(pipe));
  }

 private:
  // A random name for a pipe to be used for the test.
  const std::wstring pipe_name_{base::StrCat(
      {L"\\\\.\\pipe\\chromium.test.",
       base::ASCIIToWide(base::UnguessableToken::Create().ToString())})};
};

// Tests that FileStream::ConnectNamedPipe() works when the client has already
// opened the pipe.
TEST_F(FileStreamPipeTest, ConnectNamedPipeAfterClient) {
  base::File pipe(CreatePipe());
  ASSERT_TRUE(pipe.IsValid())
      << base::File::ErrorToString(pipe.error_details());

  FileStream pipe_stream(std::move(pipe),
                         base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_TRUE(pipe_stream.IsOpen());

  // Open the client end of the pipe.
  base::File client(OpenPipe());
  ASSERT_TRUE(client.IsValid())
      << base::File::ErrorToString(client.error_details());

  // Connecting should be synchronous and should not run the callback, but
  // handle both cases anyway for the sake of robustness against the unexpected.
  TestCompletionCallback callback;
  ASSERT_THAT(
      callback.GetResult(pipe_stream.ConnectNamedPipe(callback.callback())),
      IsOk());

  // Send some data over the pipe to be sure it works.
  scoped_refptr<IOBufferWithSize> write_io_buffer = CreateTestDataBuffer();
  int result = pipe_stream.Write(write_io_buffer.get(), write_io_buffer->size(),
                                 callback.callback());

  // Perform a synchronous read on the pipe.
  auto buffer = base::HeapArray<uint8_t>::WithSize(write_io_buffer->size());
  ASSERT_EQ(client.ReadAtCurrentPos(buffer.as_span()), write_io_buffer->size());

  // The write above may have returned ERR_IO_PENDING. Pump messages until it
  // completes, if so.
  ASSERT_THAT(callback.GetResult(result), write_io_buffer->size());
  ASSERT_EQ(buffer.as_span(), base::as_bytes(write_io_buffer->span()));
}

// Tests that FileStream::ConnectNamedPipe() works when called before the client
// has a chance to open the pipe.
TEST_F(FileStreamPipeTest, ConnectNamedPipeBeforeClient) {
  base::File pipe(CreatePipe());
  ASSERT_TRUE(pipe.IsValid())
      << base::File::ErrorToString(pipe.error_details());

  FileStream pipe_stream(std::move(pipe),
                         base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_TRUE(pipe_stream.IsOpen());

  // The client hasn't opened yet, so the connect request should wait for an
  // IO completion packet.
  TestCompletionCallback callback;
  ASSERT_THAT(pipe_stream.ConnectNamedPipe(callback.callback()),
              IsError(ERR_IO_PENDING));

  // Open the client end of the pipe.
  base::File client(OpenPipe());
  ASSERT_TRUE(client.IsValid())
      << base::File::ErrorToString(client.error_details());

  // Pump messages until the callback given to ConnectNamedPipe is run.
  ASSERT_THAT(callback.WaitForResult(), IsOk());
}

// Tests that nothing bad happens if a FileStream is destroyed after
// ConnectNamedPipe() but before a client connects.
TEST_F(FileStreamPipeTest, CloseBeforeConnect) {
  {
    base::File pipe(CreatePipe());
    ASSERT_TRUE(pipe.IsValid())
        << base::File::ErrorToString(pipe.error_details());

    FileStream pipe_stream(std::move(pipe),
                           base::SingleThreadTaskRunner::GetCurrentDefault());
    ASSERT_TRUE(pipe_stream.IsOpen());

    // The client hasn't opened yet, so the connect request should wait for an
    // IO completion packet. The callback should never be run, but it will be
    // destroyed asynchronously after the stream is closed. Give the callback a
    // `ScopedClosureRunner` that will quit the run loop when the callback is
    // destroyed.
    ASSERT_THAT(pipe_stream.ConnectNamedPipe(base::BindLambdaForTesting(
                    [loop_quitter = base::ScopedClosureRunner(QuitClosure())](
                        int error) { FAIL(); })),
                IsError(ERR_IO_PENDING));

    // Delete the FileStream; thereby cancelling the pending IO operation.
  }

  // Pump messages until the callback is destroyed following cancellation. The
  // context is still alive at this point, as a task to close the file has been
  // posted to the stream's task runner.
  RunUntilQuit();

  // Pump messages again until the task to close the file and delete the context
  // runs.
  RunUntilIdle();
}

using FileStreamPipeDeathTest = FileStreamPipeTest;

// Tests that FileStream crashes if ConnectNamedPipe() is called for a normal
// file.
TEST_F(FileStreamPipeDeathTest, CannotConnectFile) {
  const base::FilePath exe_path(base::PathService::CheckedGet(base::FILE_EXE));
  base::File exe_file(exe_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_ASYNC |
                                    base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(exe_file.IsValid())
      << base::File::ErrorToString(exe_file.error_details());

  // Pass that file to a FileStream.
  FileStream file_stream(std::move(exe_file),
                         base::SingleThreadTaskRunner::GetCurrentDefault());
  ASSERT_TRUE(file_stream.IsOpen());

  ASSERT_CHECK_DEATH(
      { file_stream.ConnectNamedPipe(CompletionOnceCallback()); });
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace net
