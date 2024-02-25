// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include <stdint.h>

#include <limits>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#endif

using net::test::IsError;
using net::test::IsOk;

namespace net {

// When the parameter is false, the UploadFileElementReader is passed only a
// FilePath and needs to open the file itself. When it's true, it's passed an
// already open base::File.
class UploadFileElementReaderTest : public testing::TestWithParam<bool>,
                                    public WithTaskEnvironment {
 protected:
  void SetUp() override {
    // Some tests (*.ReadPartially) rely on bytes_.size() being even.
    bytes_.assign({'1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c',
                   'd', 'e', 'f', 'g', 'h', 'i'});

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path_));
    ASSERT_TRUE(base::WriteFile(
        temp_file_path_, std::string_view(bytes_.data(), bytes_.size())));

    reader_ =
        CreateReader(0, std::numeric_limits<uint64_t>::max(), base::Time());

    TestCompletionCallback callback;
    ASSERT_THAT(reader_->Init(callback.callback()), IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_FALSE(reader_->IsInMemory());
  }

  ~UploadFileElementReaderTest() override {
    reader_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Creates a UploadFileElementReader based on the value of GetParam().
  std::unique_ptr<UploadFileElementReader> CreateReader(
      int64_t offset,
      int64_t length,
      base::Time expected_modification_time) {
    if (GetParam()) {
      return std::make_unique<UploadFileElementReader>(
          base::SingleThreadTaskRunner::GetCurrentDefault().get(),
          temp_file_path_, offset, length, expected_modification_time);
    }

    // The base::File::FLAG_WIN_SHARE_DELETE lets the file be deleted without
    // the test fixture waiting on it to be closed.
    int open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                     base::File::FLAG_WIN_SHARE_DELETE;
#if BUILDFLAG(IS_WIN)
    // On Windows, file must be opened for asynchronous operation.
    open_flags |= base::File::FLAG_ASYNC;
#endif  // BUILDFLAG(IS_WIN)

    base::File file(temp_file_path_, open_flags);
    EXPECT_TRUE(file.IsValid());
    return std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        std::move(file),
        // Use an incorrect path, to make sure that the file is never re-opened.
        base::FilePath(FILE_PATH_LITERAL("this_should_be_ignored")), offset,
        length, expected_modification_time);
  }

#if BUILDFLAG(IS_APPLE)
  // May be needed to avoid leaks on the Mac.
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool scoped_pool_;
#endif

  std::vector<char> bytes_;
  std::unique_ptr<UploadElementReader> reader_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
};

TEST_P(UploadFileElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  ASSERT_EQ(bytes_.size(), kHalfSize * 2);
  std::vector<char> buf(kHalfSize);
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  TestCompletionCallback read_callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback1.WaitForResult());
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin(), bytes_.begin() + kHalfSize), buf);

  TestCompletionCallback read_callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin() + kHalfSize, bytes_.end()), buf);
}

TEST_P(UploadFileElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(0,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
}

TEST_P(UploadFileElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
  EXPECT_EQ(static_cast<int>(bytes_.size()), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_P(UploadFileElementReaderTest, MultipleInit) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);

  // Read all.
  TestCompletionCallback read_callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback1.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  // Call Init() again to reset the state.
  TestCompletionCallback init_callback;
  ASSERT_THAT(reader_->Init(init_callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback.WaitForResult(), IsOk());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read again.
  TestCompletionCallback read_callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
}

TEST_P(UploadFileElementReaderTest, InitDuringAsyncOperation) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);

  // Start reading all.
  TestCompletionCallback read_callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));

  // Call Init to cancel the previous read.
  TestCompletionCallback init_callback1;
  EXPECT_THAT(reader_->Init(init_callback1.callback()),
              IsError(ERR_IO_PENDING));

  // Call Init again to cancel the previous init.
  TestCompletionCallback init_callback2;
  EXPECT_THAT(reader_->Init(init_callback2.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback2.WaitForResult(), IsOk());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read half.
  std::vector<char> buf2(bytes_.size() / 2);
  auto wrapped_buffer2 = base::MakeRefCounted<WrappedIOBuffer>(buf2);
  TestCompletionCallback read_callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer2.get(), buf2.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf2.size()), read_callback2.WaitForResult());
  EXPECT_EQ(bytes_.size() - buf2.size(), reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin(), bytes_.begin() + buf2.size()),
            buf2);

  // Make sure callbacks are not called for cancelled operations.
  EXPECT_FALSE(read_callback1.have_result());
  EXPECT_FALSE(init_callback1.have_result());
}

TEST_P(UploadFileElementReaderTest, RepeatedInitDuringInit) {
  std::vector<char> buf(bytes_.size());
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);

  TestCompletionCallback init_callback1;
  EXPECT_THAT(reader_->Init(init_callback1.callback()),
              IsError(ERR_IO_PENDING));

  // Call Init again to cancel the previous init.
  TestCompletionCallback init_callback2;
  EXPECT_THAT(reader_->Init(init_callback2.callback()),
              IsError(ERR_IO_PENDING));

  // Call Init yet again to cancel the previous init.
  TestCompletionCallback init_callback3;
  EXPECT_THAT(reader_->Init(init_callback3.callback()),
              IsError(ERR_IO_PENDING));

  EXPECT_THAT(init_callback3.WaitForResult(), IsOk());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read all.
  TestCompletionCallback read_callback;
  int result =
      reader_->Read(wrapped_buffer.get(), buf.size(), read_callback.callback());
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback.GetResult(result));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  EXPECT_FALSE(init_callback1.have_result());
  EXPECT_FALSE(init_callback2.have_result());
}

TEST_P(UploadFileElementReaderTest, Range) {
  const uint64_t kOffset = 2;
  const uint64_t kLength = bytes_.size() - kOffset * 3;
  reader_ = CreateReader(kOffset, kLength, base::Time());
  TestCompletionCallback init_callback;
  ASSERT_THAT(reader_->Init(init_callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback.WaitForResult(), IsOk());
  EXPECT_EQ(kLength, reader_->GetContentLength());
  EXPECT_EQ(kLength, reader_->BytesRemaining());
  std::vector<char> buf(kLength);
  auto wrapped_buffer = base::MakeRefCounted<WrappedIOBuffer>(buf);
  TestCompletionCallback read_callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      reader_->Read(wrapped_buffer.get(), kLength, read_callback.callback()));
  EXPECT_EQ(static_cast<int>(kLength), read_callback.WaitForResult());
  const std::vector<char> expected(bytes_.begin() + kOffset,
                                   bytes_.begin() + kOffset + kLength);
  EXPECT_EQ(expected, buf);
}

TEST_P(UploadFileElementReaderTest, FileChanged) {
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(temp_file_path_, &info));

  // Expect one second before the actual modification time to simulate change.
  const base::Time expected_modification_time =
      info.last_modified - base::Seconds(1);
  reader_ = CreateReader(0, std::numeric_limits<uint64_t>::max(),
                         expected_modification_time);
  TestCompletionCallback init_callback;
  ASSERT_THAT(reader_->Init(init_callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback.WaitForResult(), IsError(ERR_UPLOAD_FILE_CHANGED));
}

TEST_P(UploadFileElementReaderTest, InexactExpectedTimeStamp) {
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(temp_file_path_, &info));

  const base::Time expected_modification_time =
      info.last_modified - base::Milliseconds(900);
  reader_ = CreateReader(0, std::numeric_limits<uint64_t>::max(),
                         expected_modification_time);
  TestCompletionCallback init_callback;
  ASSERT_THAT(reader_->Init(init_callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback.WaitForResult(), IsOk());
}

TEST_P(UploadFileElementReaderTest, WrongPath) {
  const base::FilePath wrong_path(FILE_PATH_LITERAL("wrong_path"));
  reader_ = std::make_unique<UploadFileElementReader>(
      base::SingleThreadTaskRunner::GetCurrentDefault().get(), wrong_path, 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  TestCompletionCallback init_callback;
  ASSERT_THAT(reader_->Init(init_callback.callback()), IsError(ERR_IO_PENDING));
  EXPECT_THAT(init_callback.WaitForResult(), IsError(ERR_FILE_NOT_FOUND));
}

INSTANTIATE_TEST_SUITE_P(All,
                         UploadFileElementReaderTest,
                         testing::ValuesIn({false, true}));

}  // namespace net
