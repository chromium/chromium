// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"

#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {
namespace {
constexpr uint64_t kLargeFileSize = 1024u * 1024u * 3u;  // 3 megabytes.

// Use `ReadDataPipe()` below.
void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    base::span<const uint8_t> buffer;
    MojoResult rv = handle.BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer.data());
        EXPECT_GT(buffer.size(), 0u);
        size_t before_size = result->size();
        result->append(base::as_string_view(buffer));
        size_t read_size = result->size() - before_size;
        EXPECT_EQ(buffer.size(), read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED();
}

// Reads all data from the given `handle` and returns data as a string.
// This is similar to `mojo::BlockingCopyToString()` but a bit different. This
// doesn't wait synchronously but keeps posting a task when `handle` returns
// `MOJO_RESULT_SHOULD_WAIT`.
std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace

class FileStreamReaderToDataPipeTest : public testing::Test {
 public:
  FileStreamReaderToDataPipeTest();
  ~FileStreamReaderToDataPipeTest() override;
  void SetUp() override;

  // Creates a file filled with `file_length` random bytes.  Uses the file to
  // call `OpenFileAndReadIntoPipe()` with the given `read_length` and
  // `read_offset`.  Verifies the contents read from the pipe match the file
  // contents.
  void TestOpenFileAndReadIntoPipe(uint64_t file_length,
                                   uint64_t read_length,
                                   uint64_t read_offset);

 protected:
  // Writes `contents` to a file in the temp directory and returns the path.
  void CreateTestFile(base::span<uint8_t> contents,
                      base::FilePath* result) const;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

FileStreamReaderToDataPipeTest::FileStreamReaderToDataPipeTest() = default;

FileStreamReaderToDataPipeTest::~FileStreamReaderToDataPipeTest() = default;

void FileStreamReaderToDataPipeTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void FileStreamReaderToDataPipeTest::CreateTestFile(
    base::span<uint8_t> contents,
    base::FilePath* result) const {
  base::FilePath new_file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &new_file_path));
  ASSERT_TRUE(base::WriteFile(new_file_path, contents));
  *result = new_file_path;
}

void FileStreamReaderToDataPipeTest::TestOpenFileAndReadIntoPipe(
    uint64_t file_length,
    uint64_t read_length,
    uint64_t read_offset) {
  // Create a temporary file to read.
  std::vector<uint8_t> file_contents(file_length);
  base::RandBytes(file_contents);

  base::FilePath file_path;
  ASSERT_NO_FATAL_FAILURE(CreateTestFile(file_contents, &file_path));

  // Create the pipe to fill.
  mojo::ScopedDataPipeProducerHandle file_reader_producer;
  mojo::ScopedDataPipeConsumerHandle file_reader_consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, file_reader_producer,
                                 file_reader_consumer));

  // Read the temporary file into the pipe.
  base::RunLoop run_loop;
  content::indexed_db::OpenFileAndReadIntoPipe(
      file_path, read_offset, read_length, std::move(file_reader_producer),
      base::BindLambdaForTesting([&](int status) {
        EXPECT_EQ(status, net::OK);
        run_loop.Quit();
      }));

  std::string pipe_contents_string =
      ReadDataPipe(std::move(file_reader_consumer));
  run_loop.Run();

  // Verify the pipe contains the expected file contents.
  uint64_t expected_read_length = 0u;
  if (read_offset < file_length) {
    expected_read_length = std::min(read_length, file_length - read_offset);
  }

  std::vector<uint8_t> expected_pipe_contents(
      file_contents.begin() + read_offset,
      file_contents.begin() + read_offset + expected_read_length);

  std::vector<uint8_t> actual_pipe_contents(pipe_contents_string.begin(),
                                            pipe_contents_string.end());
  EXPECT_EQ(actual_pipe_contents, expected_pipe_contents);
}

TEST_F(FileStreamReaderToDataPipeTest, FileDoesNotExistError) {
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("file_does_not_exist");

  mojo::ScopedDataPipeProducerHandle file_reader_producer;
  mojo::ScopedDataPipeConsumerHandle file_reader_consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, file_reader_producer,
                                 file_reader_consumer));

  base::RunLoop run_loop;
  content::indexed_db::OpenFileAndReadIntoPipe(
      file_path, /*offset=*/0u, /*read_length=*/100u,
      std::move(file_reader_producer),
      base::BindLambdaForTesting([&](int status) {
        EXPECT_EQ(status, net::ERR_FILE_NOT_FOUND);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test attempts to read a directory as a file, which must fail.
TEST_F(FileStreamReaderToDataPipeTest, FileReadError) {
  mojo::ScopedDataPipeProducerHandle file_reader_producer;
  mojo::ScopedDataPipeConsumerHandle file_reader_consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, file_reader_producer,
                                 file_reader_consumer));

  base::RunLoop run_loop;
  content::indexed_db::OpenFileAndReadIntoPipe(
      temp_dir_.GetPath(), /*offset=*/0, /*read_length=*/100u,
      std::move(file_reader_producer),
      base::BindLambdaForTesting([&](int status) {
        EXPECT_NE(status, net::OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FileStreamReaderToDataPipeTest, EmptyFileRead) {
  ASSERT_NO_FATAL_FAILURE(TestOpenFileAndReadIntoPipe(
      /*file_length=*/0u, /*read_length=*/0u, /*read_offset=*/0u));
}

TEST_F(FileStreamReaderToDataPipeTest, EmptyFileReadLessThanRequested) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/0u, /*read_length=*/16u,
                              /*read_offset=*/0u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileRead) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/156u,
                              /*read_offset=*/0u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileReadStart) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/52u,
                              /*read_offset=*/0u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileReadMiddleWithOffset) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/32u,
                              /*read_offset=*/10u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileReadEndWithOffset) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/1u,
                              /*read_offset=*/155u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileReadLessThanRequestedWithOffset) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/2u,
                              /*read_offset=*/155u);
}

TEST_F(FileStreamReaderToDataPipeTest, EmptyFileReadWithOffset) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/1u,
                              /*read_offset=*/156u);
}

TEST_F(FileStreamReaderToDataPipeTest, EmptyFileReadWithTooLargeOffset) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/156u,
                              /*read_offset=*/157u);
}

TEST_F(FileStreamReaderToDataPipeTest, FileReadLessThanRequested) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/156u, /*read_length=*/157u,
                              /*read_offset=*/0u);
}

TEST_F(FileStreamReaderToDataPipeTest, LargeFileRead) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/kLargeFileSize,
                              /*read_length=*/kLargeFileSize,
                              /*read_offset=*/0u);
}

TEST_F(FileStreamReaderToDataPipeTest, LargeFileReadReadLessThanRequested) {
  TestOpenFileAndReadIntoPipe(/*file_length=*/kLargeFileSize,
                              /*read_length=*/kLargeFileSize + 1024u,
                              /*read_offset=*/0u);
}

}  // namespace content::indexed_db
