// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/buffered_file_writer.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/host/file_transfer/fake_file_operations.h"
#include "remoting/host/file_transfer/test_byte_vector_utils.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// A Writer that captures its Open callback without cancelling it on
// destruction, simulating a buggy Writer implementation that invokes a callback
// after destruction.
class StaleCallbackWriter : public FileOperations::Writer {
 public:
  explicit StaleCallbackWriter(Callback* captured_callback)
      : captured_callback_(captured_callback) {}
  ~StaleCallbackWriter() override = default;

  void Open(const base::FilePath& filename, Callback callback) override {
    state_ = FileOperations::kBusy;
    *captured_callback_ = std::move(callback);
  }
  void WriteChunk(std::vector<std::uint8_t> data, Callback callback) override {}
  void Close(Callback callback) override {}
  FileOperations::State state() const override { return state_; }

 private:
  FileOperations::State state_ = FileOperations::kCreated;
  raw_ptr<Callback> captured_callback_;
};

}  // namespace

class BufferedFileWriterTest : public testing::Test {
 public:
  BufferedFileWriterTest();
  ~BufferedFileWriterTest() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  const base::FilePath kTestFilename{FILE_PATH_LITERAL("test-file.txt")};
  const std::vector<std::uint8_t> kTestDataOne =
      ByteArrayFrom("this is the first test string");
  const std::vector<std::uint8_t> kTestDataTwo =
      ByteArrayFrom("this is the second test string");
  const std::vector<std::uint8_t> kTestDataThree =
      ByteArrayFrom("this is the third test string");

  void OnCompleted();
  void OnError(protocol::FileTransfer_Error error);

  bool complete_called_ = false;
  std::optional<protocol::FileTransfer_Error> error_ = std::nullopt;

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
  ASSERT_FALSE(complete_called_);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(complete_called_);

  ASSERT_EQ(test_io.files_written.size(), 1ul);
  ASSERT_FALSE(test_io.files_written[0].failed);
  std::vector<std::vector<std::uint8_t>> expected_chunks = {
      kTestDataOne, kTestDataTwo, kTestDataThree};
  ASSERT_EQ(test_io.files_written[0].chunks, expected_chunks);
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
  ASSERT_FALSE(complete_called_);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(complete_called_);

  ASSERT_EQ(test_io.files_written.size(), 1ul);
  ASSERT_FALSE(test_io.files_written[0].failed);
  std::vector<std::vector<std::uint8_t>> expected_chunks = {
      kTestDataOne, kTestDataTwo, kTestDataThree};
  ASSERT_EQ(test_io.files_written[0].chunks, expected_chunks);
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

  ASSERT_EQ(test_io.files_written.size(), 1ul);
  ASSERT_TRUE(test_io.files_written[0].failed);
  std::vector<std::vector<std::uint8_t>> expected_chunks = {kTestDataOne,
                                                            kTestDataTwo};
  ASSERT_EQ(test_io.files_written[0].chunks, expected_chunks);
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

    writer.Start(kTestFilename);
    writer.Write(kTestDataOne);
    writer.Write(kTestDataTwo);
    task_environment_.RunUntilIdle();
    writer.Write(kTestDataThree);
  }
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!complete_called_ && !error_);

  ASSERT_EQ(test_io.files_written.size(), 1ul);
  ASSERT_TRUE(test_io.files_written[0].failed);
  std::vector<std::vector<std::uint8_t>> expected_chunks = {kTestDataOne,
                                                            kTestDataTwo};
  ASSERT_EQ(test_io.files_written[0].chunks, expected_chunks);
}

// Verifies that if a Writer implementation invokes a callback after
// BufferedFileWriter is destroyed, the WeakPtr invalidation prevents a
// use-after-free.
TEST_F(BufferedFileWriterTest, StaleCallbackAfterDestructionIsIgnored) {
  FileOperations::Writer::Callback stale_callback;
  {
    auto underlying_writer =
        std::make_unique<StaleCallbackWriter>(&stale_callback);
    BufferedFileWriter writer(
        std::move(underlying_writer),
        base::BindOnce(
            &BufferedFileWriterTest_StaleCallbackAfterDestructionIsIgnored_Test::
                OnCompleted,
            base::Unretained(this)),
        base::BindOnce(
            &BufferedFileWriterTest_StaleCallbackAfterDestructionIsIgnored_Test::
                OnError,
            base::Unretained(this)));
    writer.Start(kTestFilename);
    ASSERT_TRUE(stale_callback);
  }

  // Invoking the captured callback after BufferedFileWriter is destroyed should
  // safely no-op.
  std::move(stale_callback)
      .Run(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR));
  EXPECT_FALSE(complete_called_);
  EXPECT_FALSE(error_);
}

}  // namespace remoting
