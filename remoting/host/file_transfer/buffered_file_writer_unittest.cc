// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/buffered_file_writer.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/host/file_transfer/fake_file_operations.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class BufferedFileWriterTest : public testing::Test {
 public:
  BufferedFileWriterTest();
  ~BufferedFileWriterTest() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  const base::FilePath kTestFilename{FILE_PATH_LITERAL("test-file.txt")};
  const std::string kTestDataOne = "this is the first test string";
  const std::string kTestDataTwo = "this is the second test string";
  const std::string kTestDataThree = "this is the third test string";

  void OnCompleted();
  void OnError(protocol::FileTransfer_Error error);

  bool complete_called_ = false;
  base::Optional<protocol::FileTransfer_Error> error_ = base::nullopt;

  base::test::TaskEnvironment task_environment_;
};

BufferedFileWriterTest::BufferedFileWriterTest() = default;

BufferedFileWriterTest::~BufferedFileWriterTest() = default;

void BufferedFileWriterTest::SetUp() {}

void BufferedFileWriterTest::TearDown() {}

void BufferedFileWriterTest::OnCompleted() {
  ASSERT_TRUE(!complete_called_ && !error_);
  complete_called_ = true;
}

void BufferedFileWriterTest::OnError(protocol::FileTransfer_Error error) {
  ASSERT_TRUE(!complete_called_ && !error_);
  error_ = std::move(error);
}

// Verifies BufferedFileWriter creates, writes to, and closes a Writer
// without errors.
TEST_F(BufferedFileWriterTest, WritesThreeChunks) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);
  BufferedFileWriter writer(
      file_operations->CreateWriter(),
      base::BindOnce(
          &BufferedFileWriterTest_WritesThreeChunks_Test::OnCompleted,
          base::Unretained(this)),
      base::BindOnce(&BufferedFileWriterTest_WritesThreeChunks_Test::OnError,
                     base::Unretained(this)));

  writer.Start(kTestFilename);
  task_environment_.RunUntilIdle();
  writer.Write(kTestDataOne);
  task_environment_.RunUntilIdle();
  writer.Write(kTestDataTwo);
  task_environment_.RunUntilIdle();
  writer.Write(kTestDataThree);
  task_environment_.RunUntilIdle();
  writer.Close();
  ASSERT_EQ(false, complete_called_);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(true, complete_called_);

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(false, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo,
                                              kTestDataThree};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);
}

// Verifies BufferedFileWriter properly queues up file operations.
TEST_F(BufferedFileWriterTest, QueuesOperations) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);
  BufferedFileWriter writer(
      file_operations->CreateWriter(),
      base::BindOnce(&BufferedFileWriterTest_QueuesOperations_Test::OnCompleted,
                     base::Unretained(this)),
      base::BindOnce(&BufferedFileWriterTest_QueuesOperations_Test::OnError,
                     base::Unretained(this)));

  // FakeFileWriter will CHECK that BufferedFileWriter properly serializes
  // file operations.
  writer.Start(kTestFilename);
  writer.Write(kTestDataOne);
  writer.Write(kTestDataTwo);
  writer.Write(kTestDataThree);
  writer.Close();
  ASSERT_EQ(false, complete_called_);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(true, complete_called_);

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(false, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo,
                                              kTestDataThree};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);
}

// Verifies BufferedFileWriter calls the error callback in the event of an
// error.
TEST_F(BufferedFileWriterTest, HandlesWriteError) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);
  BufferedFileWriter writer(
      file_operations->CreateWriter(),
      base::BindOnce(
          &BufferedFileWriterTest_HandlesWriteError_Test::OnCompleted,
          base::Unretained(this)),
      base::BindOnce(&BufferedFileWriterTest_HandlesWriteError_Test::OnError,
                     base::Unretained(this)));
  protocol::FileTransfer_Error fake_error = protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR);

  writer.Start(kTestFilename);
  writer.Write(kTestDataOne);
  writer.Write(kTestDataTwo);
  task_environment_.RunUntilIdle();
  test_io.io_error = fake_error;
  writer.Write(kTestDataThree);
  writer.Close();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(error_);
  ASSERT_EQ(fake_error.SerializeAsString(), error_->SerializeAsString());

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(true, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);
}

// Verifies canceling BufferedFileWriter cancels the underlying writer.
TEST_F(BufferedFileWriterTest, CancelsWriter) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);
  {
    BufferedFileWriter writer(
        file_operations->CreateWriter(),
        base::BindOnce(&BufferedFileWriterTest_CancelsWriter_Test::OnCompleted,
                       base::Unretained(this)),
        base::BindOnce(&BufferedFileWriterTest_CancelsWriter_Test::OnError,
                       base::Unretained(this)));
    protocol::FileTransfer_Error fake_error = protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR);

    writer.Start(kTestFilename);
    writer.Write(kTestDataOne);
    writer.Write(kTestDataTwo);
    task_environment_.RunUntilIdle();
    writer.Write(kTestDataThree);
  }
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!complete_called_ && !error_);

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(true, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);
}

}  // namespace remoting
