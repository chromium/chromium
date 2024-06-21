// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_data_pipe_writer.h"

#include <optional>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const std::string kTestData = "hello world";
const std::string kTestData1 = "hello ";
const std::string kTestData2 = "world";

class DummySharedDictionaryWriter : public SharedDictionaryWriter {
 public:
  DummySharedDictionaryWriter() = default;

  DummySharedDictionaryWriter(const DummySharedDictionaryWriter&) = delete;
  DummySharedDictionaryWriter& operator=(const DummySharedDictionaryWriter&) =
      delete;

  // SharedDictionaryWriter
  void Append(const char* buf, int num_bytes) override {
    data_.emplace_back(buf, num_bytes);
  }
  void Finish() override { finished_ = true; }

  const std::vector<std::string> data() const { return data_; }
  bool finished() const { return finished_; }

 private:
  ~DummySharedDictionaryWriter() override = default;

  std::vector<std::string> data_;
  bool finished_ = false;
};

}  // namespace

class SharedDictionaryDataPipeWriterTest : public ::testing::Test {
 public:
  SharedDictionaryDataPipeWriterTest()
      : finish_result_(
            base::MakeRefCounted<base::RefCountedData<std::optional<bool>>>()),
        dummy_writer_(base::MakeRefCounted<DummySharedDictionaryWriter>()) {
    CreateDataPipe(producer_handle_, consumer_handle_);
  }
  ~SharedDictionaryDataPipeWriterTest() override = default;

  SharedDictionaryDataPipeWriterTest(
      const SharedDictionaryDataPipeWriterTest&) = delete;
  SharedDictionaryDataPipeWriterTest& operator=(
      const SharedDictionaryDataPipeWriterTest&) = delete;

 protected:
  static uint32_t GetDataPipeBufferSize() {
    return SharedDictionaryDataPipeWriter::GetDataPipeBufferSize();
  }

  static void CreateDataPipe(
      mojo::ScopedDataPipeProducerHandle& producer_handle,
      mojo::ScopedDataPipeConsumerHandle& consumer_handle) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = GetDataPipeBufferSize();
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, producer_handle, consumer_handle));
  }

  void CreateDataPipeWriter() {
    data_pipe_writer_ = SharedDictionaryDataPipeWriter::Create(
        consumer_handle_, dummy_writer_,
        base::BindOnce(
            [](scoped_refptr<base::RefCountedData<std::optional<bool>>>
                   finish_result,
               bool result) { finish_result->data = result; },
            finish_result_));
  }

  std::string GetDataInComsumerHandle(bool consume = false) {
    std::string output;
    base::span<const uint8_t> buffer;
    MojoResult result =
        consumer_handle_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (result == MOJO_RESULT_FAILED_PRECONDITION ||
        result == MOJO_RESULT_SHOULD_WAIT) {
      return output;
    }
    CHECK_EQ(MOJO_RESULT_OK, result);
    output = std::string(base::as_string_view(buffer));
    consumer_handle_->EndReadData(consume ? buffer.size() : 0);
    return output;
  }

  // Set when `data_pipe_writer_`'s finish_callback is called.
  scoped_refptr<base::RefCountedData<std::optional<bool>>> finish_result_;

  // The data flow looks like:
  //   `producer_handle_` --> `data_pipe_writer_` --> `consumer_handle_`
  //                                  |
  //                                  +-> `dummy_writer_`
  scoped_refptr<DummySharedDictionaryWriter> dummy_writer_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<SharedDictionaryDataPipeWriter> data_pipe_writer_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SharedDictionaryDataPipeWriterTest, EmptyInput) {
  CreateDataPipeWriter();

  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // No data must be written.
  EXPECT_TRUE(dummy_writer_->data().empty());
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ("", GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, OneDataInput) {
  CreateDataPipeWriter();

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData, GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, DelayedOnComplete) {
  CreateDataPipeWriter();

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  // End of input data.
  producer_handle_.reset();

  task_environment_.RunUntilIdle();

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData, GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, OnCompleteAndEndOfInput) {
  CreateDataPipeWriter();

  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  // End of input data.
  producer_handle_.reset();

  task_environment_.RunUntilIdle();

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData, GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, EndOfInputAndOnCompleteFailure) {
  CreateDataPipeWriter();

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  // End of input data.
  producer_handle_.reset();

  task_environment_.RunUntilIdle();

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Failure from upstream.
  data_pipe_writer_->OnComplete(/*success=*/false);

  // Finish callback must be called with false.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData, GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, OnCompleteFailureAndEndOfInput) {
  CreateDataPipeWriter();

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  task_environment_.RunUntilIdle();

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Failure from upstream.
  data_pipe_writer_->OnComplete(/*success=*/false);

  // Finish callback must be called with false.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData, GetDataInComsumerHandle());

  // End of input data.
  producer_handle_.reset();

  task_environment_.RunUntilIdle();

  // The writer must not be finished.
  EXPECT_FALSE(dummy_writer_->finished());
}

TEST_F(SharedDictionaryDataPipeWriterTest, TwoDataInputOneTimeRead) {
  CreateDataPipeWriter();

  // Write the first input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData1, producer_handle_));
  // Write the second input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData2, producer_handle_));
  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The data must be written.
  EXPECT_EQ(base::StrCat({kTestData1, kTestData2}),
            base::StrCat(dummy_writer_->data()));
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(base::StrCat({kTestData1, kTestData2}), GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, TwoDataInputTwoTimeRead) {
  CreateDataPipeWriter();

  // Write the first input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData1, producer_handle_));

  task_environment_.RunUntilIdle();

  // The first data must be written.
  EXPECT_EQ(kTestData1, base::StrCat(dummy_writer_->data()));
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(kTestData1, GetDataInComsumerHandle());

  // Write the second input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData2, producer_handle_));

  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The data must be written.
  EXPECT_EQ(base::StrCat({kTestData1, kTestData2}),
            base::StrCat(dummy_writer_->data()));
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(base::StrCat({kTestData1, kTestData2}), GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, DataBeforeCreatingDataPipeWriter) {
  // Write the data before creating a SharedDictionaryDataPipeWriter.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));

  CreateDataPipeWriter();

  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The data must be written.
  EXPECT_EQ(kTestData, base::StrCat(dummy_writer_->data()));
  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());

  // Check the data in the output data pipe.
  EXPECT_EQ(base::StrCat({kTestData1, kTestData2}), GetDataInComsumerHandle());
}

TEST_F(SharedDictionaryDataPipeWriterTest, PeerClosed) {
  CreateDataPipeWriter();

  // Resetting `consumer_handle_` will be treated as aborted.
  consumer_handle_.reset();

  task_environment_.RunUntilIdle();

  // Finish callback must be called with false
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The data must not be written.
  EXPECT_TRUE(dummy_writer_->data().empty());
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());
}

TEST_F(SharedDictionaryDataPipeWriterTest, ReadAndPeerClosed) {
  CreateDataPipeWriter();

  // Write the input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));

  // Resetting `consumer_handle_` will be treated as aborted.
  consumer_handle_.reset();

  task_environment_.RunUntilIdle();

  // Finish callback must be called with false
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The data must not be written.
  EXPECT_TRUE(dummy_writer_->data().empty());
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());
}

TEST_F(SharedDictionaryDataPipeWriterTest, PeerClosedAndOnCompleteFailure) {
  CreateDataPipeWriter();

  // Resetting `consumer_handle_` will be treated as aborted.
  consumer_handle_.reset();

  task_environment_.RunUntilIdle();

  // Finish callback must be called with false
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The data must not be written.
  EXPECT_TRUE(dummy_writer_->data().empty());
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Failure from upstream.
  data_pipe_writer_->OnComplete(/*success=*/false);
  task_environment_.RunUntilIdle();

  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());
}

TEST_F(SharedDictionaryDataPipeWriterTest, PeerClosedAndOnComplete) {
  CreateDataPipeWriter();

  // Resetting `consumer_handle_` will be treated as aborted.
  consumer_handle_.reset();

  task_environment_.RunUntilIdle();

  // Finish callback must be called with false
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_FALSE(*finish_result_->data);

  // The data must not be written.
  EXPECT_TRUE(dummy_writer_->data().empty());
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());
}

TEST_F(SharedDictionaryDataPipeWriterTest, WriteDataWait) {
  CreateDataPipeWriter();

  const std::string kTestBigData =
      std::string(GetDataPipeBufferSize() / 2, 'x');

  // Write the first big input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestBigData, producer_handle_));
  // Write the first big input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestBigData, producer_handle_));

  task_environment_.RunUntilIdle();

  // The first and the second data must be written.
  EXPECT_EQ(base::StrCat({kTestBigData, kTestBigData}),
            base::StrCat(dummy_writer_->data()));
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // Write the third big input data.
  ASSERT_TRUE(mojo::BlockingCopyFromString(kTestBigData, producer_handle_));

  task_environment_.RunUntilIdle();

  // SharedDictionaryDataPipeWriter can't write new data to the mojo data pipe
  // 'SharedDictionaryDataPipeWriter::producer_handle_' because it is full.
  EXPECT_EQ(base::StrCat({kTestBigData, kTestBigData}),
            base::StrCat(dummy_writer_->data()));
  EXPECT_FALSE(dummy_writer_->finished());

  // Check the data in the output data pipe, and consume the data in the data
  // pipe.
  EXPECT_EQ(base::StrCat({kTestBigData, kTestBigData}),
            GetDataInComsumerHandle(/*consume=*/true));

  task_environment_.RunUntilIdle();

  // The third data must be written.
  EXPECT_EQ(base::StrCat({kTestBigData, kTestBigData, kTestBigData}),
            base::StrCat(dummy_writer_->data()));
  // The writer has not finished.
  EXPECT_FALSE(dummy_writer_->finished());

  // End of input data.
  producer_handle_.reset();
  // Success signal from upstream.
  data_pipe_writer_->OnComplete(/*success=*/true);

  task_environment_.RunUntilIdle();

  // Finish callback must be called with true.
  ASSERT_TRUE(finish_result_->data.has_value());
  EXPECT_TRUE(*finish_result_->data);

  // The writer must be finished.
  EXPECT_TRUE(dummy_writer_->finished());
}

}  // namespace network
