// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/local_file_operations.h"

#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "remoting/host/file_transfer/directory_helpers.h"
#include "remoting/host/file_transfer/ensure_user.h"
#include "remoting/host/file_transfer/fake_file_chooser.h"
#include "remoting/host/file_transfer/test_byte_vector_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// BindOnce disallows binding lambdas with captures. This is reasonable in
// production code, as it requires one to either explicitly pass owned objects
// or pointers using Owned, Unretained, et cetera. This helps to avoid use-
// after-free bugs in async code. In test code, though, where the lambda is
// immediately invoked in the test method using, e.g., RunUntilIdle, the ability
// to capture can make the code much easier to read and write.
template <typename T>
auto BindLambda(T lambda) {
  return base::BindOnce(&T::operator(),
                        base::Owned(new auto(std::move(lambda))));
}

}  // namespace

class LocalFileOperationsTest : public testing::Test {
 public:
  LocalFileOperationsTest();

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  const base::FilePath kTestFilename =
      base::FilePath::FromUTF8Unsafe("test-file.txt");
  const base::FilePath kTestFilenameSecondary =
      base::FilePath::FromUTF8Unsafe("test-file (1).txt");
  const base::FilePath kTestFilenameTertiary =
      base::FilePath::FromUTF8Unsafe("test-file (2).txt");
  const std::vector<std::uint8_t> kTestDataOne =
      ByteArrayFrom("this is the first test string");
  const std::vector<std::uint8_t> kTestDataTwo =
      ByteArrayFrom("this is the second test string");
  const std::vector<std::uint8_t> kTestDataThree =
      ByteArrayFrom("this is the third test string");
  const std::vector<std::uint8_t> kTestDataSmallTail = ByteArrayFrom("string4");

  base::FilePath TestDir();
  void WriteFile(const base::FilePath& filename,
                 base::queue<std::vector<uint8_t>> chunks,
                 bool close);
  void OnOperationComplete(base::queue<std::vector<uint8_t>> remaining_chunks,
                           bool close,
                           FileOperations::Writer::Result result);
  void OnCloseComplete(FileOperations::Writer::Result result);

  // Points DIR_USER_DESKTOP at a scoped temporary directory.
  base::ScopedPathOverride scoped_path_override_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FileOperations> file_operations_;
  std::unique_ptr<FileOperations::Writer> file_writer_;
  bool operation_completed_ = false;
};

LocalFileOperationsTest::LocalFileOperationsTest()
    : scoped_path_override_(base::DIR_USER_DESKTOP),
      task_environment_(
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
      file_operations_(std::make_unique<LocalFileOperations>(
          task_environment_.GetMainThreadTaskRunner())) {}

void LocalFileOperationsTest::SetUp() {
  DisableUserContextCheckForTesting();
  SetFileUploadDirectoryForTesting(TestDir());
}

void LocalFileOperationsTest::TearDown() {}

base::FilePath LocalFileOperationsTest::TestDir() {
  base::FilePath result;
  EXPECT_TRUE(base::PathService::Get(base::DIR_USER_DESKTOP, &result));
  return result;
}

void LocalFileOperationsTest::WriteFile(
    const base::FilePath& filename,
    base::queue<std::vector<std::uint8_t>> chunks,
    bool close) {
  operation_completed_ = false;
  file_writer_ = file_operations_->CreateWriter();
  file_writer_->Open(
      filename,
      base::BindOnce(&LocalFileOperationsTest::OnOperationComplete,
                     base::Unretained(this), std::move(chunks), close));
}

void LocalFileOperationsTest::OnOperationComplete(
    base::queue<std::vector<std::uint8_t>> remaining_chunks,
    bool close,
    FileOperations::Writer::Result result) {
  ASSERT_TRUE(result);
  if (!remaining_chunks.empty()) {
    std::vector<std::uint8_t> next_chunk = std::move(remaining_chunks.front());
    remaining_chunks.pop();
    file_writer_->WriteChunk(
        std::move(next_chunk),
        base::BindOnce(&LocalFileOperationsTest::OnOperationComplete,
                       base::Unretained(this), std::move(remaining_chunks),
                       close));
  } else if (close) {
    file_writer_->Close(base::BindOnce(
        &LocalFileOperationsTest::OnCloseComplete, base::Unretained(this)));
  } else {
    operation_completed_ = true;
  }
}

void LocalFileOperationsTest::OnCloseComplete(
    FileOperations::Writer::Result result) {
  ASSERT_TRUE(result);
  operation_completed_ = true;
}

// Verifies that a file consisting of three chunks can be written successfully.
TEST_F(LocalFileOperationsTest, WritesThreeChunks) {
  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>(
                {kTestDataOne, kTestDataTwo, kTestDataThree}),
            true /* close */);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  std::string actual_file_data;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data));
  ASSERT_EQ(ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree),
            ByteArrayFrom(actual_file_data));
}

// Verifies that a file with a small last chunk can be written successfully.
TEST_F(LocalFileOperationsTest, WritesSmallTail) {
  WriteFile(
      kTestFilename,
      base::queue<std::vector<std::uint8_t>>(
          {kTestDataOne, kTestDataTwo, kTestDataThree, kTestDataSmallTail}),
      true /* close */);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  std::string actual_file_data;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data));
  ASSERT_EQ(ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree,
                          kTestDataSmallTail),
            ByteArrayFrom(actual_file_data));
}

// Verifies that LocalFileOperations will write to a file named "file (1).txt"
// if "file.txt" already exists, and "file (2).txt" after that.
TEST_F(LocalFileOperationsTest, RenamesFileIfExists) {
  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>({kTestDataOne}),
            true /* close */);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(operation_completed_);

  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>({kTestDataTwo}),
            true /* close */);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(operation_completed_);

  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>({kTestDataThree}),
            true /* close */);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(operation_completed_);

  std::string actual_file_data_one;
  EXPECT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data_one));
  EXPECT_EQ(kTestDataOne, ByteArrayFrom(actual_file_data_one));
  std::string actual_file_data_two;
  EXPECT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilenameSecondary),
                                     &actual_file_data_two));
  EXPECT_EQ(kTestDataTwo, ByteArrayFrom(actual_file_data_two));
  std::string actual_file_data_three;
  EXPECT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilenameTertiary),
                                     &actual_file_data_three));
  EXPECT_EQ(kTestDataThree, ByteArrayFrom(actual_file_data_three));
}

// Verifies that dropping early deletes the temporary file.
TEST_F(LocalFileOperationsTest, DroppingDeletesTemp) {
  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>(
                {kTestDataOne, kTestDataTwo, kTestDataThree}),
            false /* close */);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  file_writer_.reset();
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that dropping works while an operation is pending.
TEST_F(LocalFileOperationsTest, CancelsWhileOperationPending) {
  WriteFile(kTestFilename,
            base::queue<std::vector<std::uint8_t>>({kTestDataOne}),
            false /* close */);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  file_writer_->WriteChunk(kTestDataTwo, base::DoNothing());
  file_writer_.reset();
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that a file can be successfully opened for reading.
TEST_F(LocalFileOperationsTest, OpensReader) {
  base::FilePath path = TestDir().Append(kTestFilename);
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  ASSERT_TRUE(base::WriteFile(path, contents));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  ASSERT_EQ(FileOperations::kCreated, reader->state());
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  ASSERT_EQ(FileOperations::kBusy, reader->state());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  EXPECT_EQ(kTestFilename, reader->filename());
  EXPECT_EQ(contents.size(), reader->size());
}

// Verifies that a file can be successfully read in three chunks.
TEST_F(LocalFileOperationsTest, ReadsThreeChunks) {
  base::FilePath path = TestDir().Append(kTestFilename);
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  ASSERT_TRUE(base::WriteFile(path, contents));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    std::optional<FileOperations::Reader::ReadResult> read_result;
    reader->ReadChunk(
        chunk.size(),
        BindLambda([&](FileOperations::Reader::ReadResult result) {
          read_result = std::move(result);
        }));
    ASSERT_EQ(FileOperations::kBusy, reader->state());
    task_environment_.RunUntilIdle();
    ASSERT_EQ(FileOperations::kReady, reader->state());
    ASSERT_TRUE(read_result);
    ASSERT_TRUE(*read_result);
    EXPECT_EQ(chunk, **read_result);
  }
}

// Verifies proper EOF handling.
TEST_F(LocalFileOperationsTest, ReaderHandlesEof) {
  base::FilePath path = TestDir().Append(kTestFilename);
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  ASSERT_TRUE(base::WriteFile(path, contents));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(
      contents.size() + 5,  // Attempt to read more than is in file.
      BindLambda([&](FileOperations::Reader::ReadResult result) {
        read_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(read_result && *read_result);
  EXPECT_EQ(contents, **read_result);

  read_result.reset();
  reader->ReadChunk(5,
                    BindLambda([&](FileOperations::Reader::ReadResult result) {
                      read_result = std::move(result);
                    }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, reader->state());
  ASSERT_TRUE(read_result && *read_result);
  EXPECT_EQ(std::size_t{0}, (*read_result)->size());
}

// Verifies cancellation is propagated.
TEST_F(LocalFileOperationsTest, ReaderCancels) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_FALSE(*open_result);
  EXPECT_EQ(protocol::FileTransfer_Error_Type_CANCELED,
            open_result->error().type());
}

// Verifies failure when file doesn't exist.
TEST_F(LocalFileOperationsTest, FileNotFound) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  // Currently non-existent file.
  FakeFileChooser::SetResult(TestDir().Append(kTestFilename));
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_FALSE(*open_result);
}

}  // namespace remoting
