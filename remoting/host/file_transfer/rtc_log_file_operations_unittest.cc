// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/rtc_log_file_operations.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/webrtc_event_log_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

std::string ToString(const std::vector<std::uint8_t>& data) {
  return std::string(data.begin(), data.end());
}

class FakeConnectionWithRtcLog : public protocol::FakeConnectionToClient {
 public:
  explicit FakeConnectionWithRtcLog(protocol::WebrtcEventLogData* event_log)
      : protocol::FakeConnectionToClient(nullptr), event_log_(event_log) {}
  ~FakeConnectionWithRtcLog() override = default;
  FakeConnectionWithRtcLog(const FakeConnectionWithRtcLog&) = delete;
  FakeConnectionWithRtcLog& operator=(const FakeConnectionWithRtcLog&) = delete;

  protocol::WebrtcEventLogData* rtc_event_log() override { return event_log_; }

  void set_event_log(protocol::WebrtcEventLogData* log) { event_log_ = log; }

 private:
  raw_ptr<protocol::WebrtcEventLogData> event_log_;
};

}  // namespace

class RtcLogFileOperationsTest : public testing::Test {
 public:
  RtcLogFileOperationsTest()
      : connection_(&event_log_), file_operations_(&connection_) {}

  void SetUp() override { reader_ = file_operations_.CreateReader(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  protocol::WebrtcEventLogData event_log_;
  FakeConnectionWithRtcLog connection_;
  RtcLogFileOperations file_operations_;
  std::unique_ptr<FileOperations::Reader> reader_;

  // These are the most-recent results from the callbacks.
  std::optional<FileOperations::Reader::OpenResult> open_result_;
  std::optional<FileOperations::Reader::ReadResult> read_result_;

  FileOperations::Reader::OpenCallback MakeOpenCallback() {
    return base::BindOnce(&RtcLogFileOperationsTest::OnOpenResult,
                          base::Unretained(this));
  }

  FileOperations::Reader::ReadCallback MakeReadCallback() {
    return base::BindOnce(&RtcLogFileOperationsTest::OnReadResult,
                          base::Unretained(this));
  }

 private:
  void OnOpenResult(FileOperations::Reader::OpenResult result) {
    open_result_ = result;
  }

  void OnReadResult(FileOperations::Reader::ReadResult result) {
    read_result_ = result;
  }
};

TEST_F(RtcLogFileOperationsTest, InitialState_IsCreated) {
  EXPECT_EQ(reader_->state(), FileOperations::State::kCreated);
}

TEST_F(RtcLogFileOperationsTest, NonWebrtcConnection_RaisesProtocolError) {
  connection_.set_event_log(nullptr);

  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(open_result_);
  ASSERT_FALSE(*open_result_);
  EXPECT_EQ(open_result_->error().type(),
            protocol::FileTransfer_Error_Type_PROTOCOL_ERROR);
}

TEST_F(RtcLogFileOperationsTest, EmptyLog_ZeroBytesSent) {
  // No setup needed, the RTC log is initially empty.
  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();
  reader_->ReadChunk(1U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(open_result_);
  ASSERT_TRUE(*open_result_);
  ASSERT_TRUE(read_result_);
  ASSERT_TRUE(*read_result_);

  EXPECT_TRUE((*read_result_)->empty());
  EXPECT_EQ(reader_->state(), FileOperations::State::kComplete);
}

TEST_F(RtcLogFileOperationsTest, FileSizeOfMultipleLogSections_IsCorrect) {
  event_log_.SetMaxSectionSizeForTest(3);
  event_log_.Write("aaa");
  event_log_.Write("bb");
  event_log_.Write("cc");

  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(reader_->size(), 7U);
}

TEST_F(RtcLogFileOperationsTest, PartialTransfer_IsReady) {
  event_log_.Write("aaa");

  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();
  reader_->ReadChunk(2U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(reader_->state(), FileOperations::State::kReady);
}

TEST_F(RtcLogFileOperationsTest, CompleteTransfer_IsComplete) {
  event_log_.Write("aaa");

  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();
  reader_->ReadChunk(3U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  // State becomes Complete only after a final read has returned 0 bytes.
  reader_->ReadChunk(3U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(read_result_);
  ASSERT_TRUE(*read_result_);
  EXPECT_TRUE((*read_result_)->empty());
  EXPECT_EQ(reader_->state(), FileOperations::State::kComplete);
}

TEST_F(RtcLogFileOperationsTest,
       MultipleReadsOverMultipleSections_CorrectDataIsSent) {
  event_log_.SetMaxSectionSizeForTest(3);
  event_log_.Write("aaa");
  event_log_.Write("bbb");
  event_log_.Write("ccc");

  reader_->Open(MakeOpenCallback());
  task_environment_.RunUntilIdle();
  reader_->ReadChunk(2U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(read_result_);
  ASSERT_TRUE(*read_result_);
  EXPECT_EQ(ToString(**read_result_), "aa");

  reader_->ReadChunk(5U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(ToString(**read_result_), "abbbc");

  reader_->ReadChunk(100U, MakeReadCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(ToString(**read_result_), "cc");
}

}  // namespace remoting
