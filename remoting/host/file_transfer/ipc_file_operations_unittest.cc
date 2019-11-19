// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ipc_file_operations.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "remoting/host/file_transfer/fake_file_chooser.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/file_transfer/session_file_operations_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// BindOnce disallows binding lambdas with captures. This is reasonable in
// production code, as it requires one to either explicitly pass owned objects
// or pointers using Owned, Unretained, et cetera. This helps to avoid use after
// free bugs in async code. In test code, though, where the lambda is
// immediately invoked in the test method using, e.g., RunUntilIdle, the ability
// to capture can make the code much easier to read and write.
template <typename T>
auto BindLambda(T lambda) {
  return base::BindOnce(&T::operator(),
                        base::Owned(new auto(std::move(lambda))));
}

class IpcTestBridge : public IpcFileOperations::RequestHandler,
                      public IpcFileOperations::ResultHandler,
                      public FileOperations {
 public:
  explicit IpcTestBridge(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : ipc_file_operations_factory_(this),
        session_file_operations_handler_(
            this,
            std::make_unique<LocalFileOperations>(std::move(ui_task_runner))),
        file_operations_(ipc_file_operations_factory_.CreateFileOperations()) {}

  ~IpcTestBridge() override = default;

  // IpcFileOperations::RequestHandler implementation.
  void ReadFile(std::uint64_t file_id) override {
    session_file_operations_handler_.ReadFile(file_id);
  }
  void ReadChunk(std::uint64_t file_id, std::uint64_t size) override {
    session_file_operations_handler_.ReadChunk(file_id, size);
  }
  void WriteFile(std::uint64_t file_id,
                 const base::FilePath& filename) override {
    session_file_operations_handler_.WriteFile(file_id, filename);
  }
  void WriteChunk(std::uint64_t file_id, std::string data) override {
    session_file_operations_handler_.WriteChunk(file_id, std::move(data));
  }
  void Close(std::uint64_t file_id) override {
    session_file_operations_handler_.Close(file_id);
  }
  void Cancel(std::uint64_t file_id) override {
    session_file_operations_handler_.Cancel(file_id);
  }

  // ResultHandler implementation.
  void OnResult(std::uint64_t file_id, Result result) override {
    ipc_file_operations_factory_.OnResult(file_id, std::move(result));
  }
  void OnInfoResult(std::uint64_t file_id, InfoResult result) override {
    ipc_file_operations_factory_.OnInfoResult(file_id, std::move(result));
  }
  void OnDataResult(std::uint64_t file_id, DataResult result) override {
    ipc_file_operations_factory_.OnDataResult(file_id, std::move(result));
  }

  // FileOperations implementation.
  std::unique_ptr<Reader> CreateReader() override {
    return file_operations_->CreateReader();
  }
  std::unique_ptr<Writer> CreateWriter() override {
    return file_operations_->CreateWriter();
  }

 private:
  IpcFileOperationsFactory ipc_file_operations_factory_;
  SessionFileOperationsHandler session_file_operations_handler_;
  std::unique_ptr<FileOperations> file_operations_;

  DISALLOW_COPY_AND_ASSIGN(IpcTestBridge);
};

}  // namespace

class IpcFileOperationsTest : public testing::Test {
 public:
  IpcFileOperationsTest();
  ~IpcFileOperationsTest() override;

 protected:
  const base::FilePath kTestFilename =
      base::FilePath::FromUTF8Unsafe("test-file.txt");
  const std::string kTestDataOne = "this is the first test string";
  const std::string kTestDataTwo = "this is the second test string";
  const std::string kTestDataThree = "this is the third test string";

  base::FilePath TestDir();

  // Points DIR_USER_DESKTOP at a scoped temporary directory.
  base::ScopedPathOverride scoped_path_override_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FileOperations> file_operations_;

  DISALLOW_COPY_AND_ASSIGN(IpcFileOperationsTest);
};

IpcFileOperationsTest::IpcFileOperationsTest()
    : scoped_path_override_(base::DIR_USER_DESKTOP),
      task_environment_(
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
      file_operations_(std::make_unique<IpcTestBridge>(
          task_environment_.GetMainThreadTaskRunner())) {}

IpcFileOperationsTest::~IpcFileOperationsTest() = default;

base::FilePath IpcFileOperationsTest::TestDir() {
  base::FilePath result;
  EXPECT_TRUE(base::PathService::Get(base::DIR_USER_DESKTOP, &result));
  return result;
}

// Verifies that a file consisting of three chunks can be written successfully.
TEST_F(IpcFileOperationsTest, WritesThreeChunks) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  ASSERT_EQ(FileOperations::kCreated, writer->state());

  base::Optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename,
               BindLambda([&](FileOperations::Writer::Result result) {
                 open_result = std::move(result);
               }));
  ASSERT_EQ(FileOperations::kBusy, writer->state());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, writer->state());
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    base::Optional<FileOperations::Writer::Result> write_result;
    writer->WriteChunk(chunk,
                       BindLambda([&](FileOperations::Writer::Result result) {
                         write_result = std::move(result);
                       }));
    ASSERT_EQ(FileOperations::kBusy, writer->state());
    task_environment_.RunUntilIdle();
    ASSERT_EQ(FileOperations::kReady, writer->state());
    ASSERT_TRUE(write_result);
    ASSERT_TRUE(*write_result);
  }

  base::Optional<FileOperations::Writer::Result> close_result;
  writer->Close(BindLambda([&](FileOperations::Writer::Result result) {
    close_result = std::move(result);
  }));
  ASSERT_EQ(FileOperations::kBusy, writer->state());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, writer->state());

  std::string actual_file_data;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data));
  EXPECT_EQ(kTestDataOne + kTestDataTwo + kTestDataThree, actual_file_data);
}

// Verifies that dropping early cancels the remote writer.
TEST_F(IpcFileOperationsTest, DroppingCancelsRemote) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  base::Optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename,
               BindLambda([&](FileOperations::Writer::Result result) {
                 open_result = std::move(result);
               }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    base::Optional<FileOperations::Writer::Result> write_result;
    writer->WriteChunk(chunk,
                       BindLambda([&](FileOperations::Writer::Result result) {
                         write_result = std::move(result);
                       }));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(write_result && *write_result);
  }

  writer.reset();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that dropping works while an operation is pending.
TEST_F(IpcFileOperationsTest, CancelsWhileOperationPending) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  base::Optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename,
               BindLambda([&](FileOperations::Writer::Result result) {
                 open_result = std::move(result);
               }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  base::Optional<FileOperations::Writer::Result> write_result;
  writer->WriteChunk(std::string(kTestDataOne),
                     BindLambda([&](FileOperations::Writer::Result result) {
                       write_result = std::move(result);
                     }));

  EXPECT_EQ(FileOperations::kBusy, writer->state());
  writer.reset();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(write_result);
  EXPECT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that a file can be successfully read in three chunks.
TEST_F(IpcFileOperationsTest, ReadsThreeChunks) {
  base::FilePath path = TestDir().Append(kTestFilename);
  std::string contents = kTestDataOne + kTestDataTwo + kTestDataThree;
  ASSERT_EQ(static_cast<int>(contents.size()),
            base::WriteFile(path, contents.data(), contents.size()));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  ASSERT_EQ(FileOperations::kCreated, reader->state());

  FakeFileChooser::SetResult(path);
  base::Optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  ASSERT_EQ(FileOperations::kBusy, reader->state());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    base::Optional<FileOperations::Reader::ReadResult> read_result;
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
TEST_F(IpcFileOperationsTest, ReaderHandlesEof) {
  constexpr std::size_t kOverreadAmount = 5;
  base::FilePath path = TestDir().Append(kTestFilename);
  std::string contents = kTestDataOne + kTestDataTwo + kTestDataThree;
  ASSERT_EQ(static_cast<int>(contents.size()),
            base::WriteFile(path, contents.data(), contents.size()));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  base::Optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  base::Optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(
      contents.size() +
          kOverreadAmount,  // Attempt to read more than is in file.
      BindLambda([&](FileOperations::Reader::ReadResult result) {
        read_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(contents, **read_result);

  read_result.reset();
  reader->ReadChunk(kOverreadAmount,
                    BindLambda([&](FileOperations::Reader::ReadResult result) {
                      read_result = std::move(result);
                    }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(std::size_t{0}, (*read_result)->size());
}

// Verifies proper handling of zero-size file
TEST_F(IpcFileOperationsTest, ReaderHandlesZeroSize) {
  constexpr std::size_t kChunkSize = 5;
  base::FilePath path = TestDir().Append(kTestFilename);
  ASSERT_EQ(0, base::WriteFile(path, "", 0));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  base::Optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result && *open_result);

  base::Optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(kChunkSize,
                    BindLambda([&](FileOperations::Reader::ReadResult result) {
                      read_result = std::move(result);
                    }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(std::size_t{0}, (*read_result)->size());
}

// Verifies error is propagated.
TEST_F(IpcFileOperationsTest, ReaderPropagatesError) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  // Currently non-existent file.
  FakeFileChooser::SetResult(TestDir().Append(kTestFilename));
  base::Optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(BindLambda([&](FileOperations::Reader::OpenResult result) {
    open_result = std::move(result);
  }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_FALSE(*open_result);
}

}  // namespace remoting
