// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/data_pipe_to_source_stream.h"

#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const int kBigBufferSize = 4096;
const int kSmallBufferSize = 1;
const int kBigPipeCapacity = 4096;
const int kSmallPipeCapacity = 1;

enum class WriteMode {
  // WriteToPipe tries to write as many bytes as possible.
  EVERYTHING_AT_ONCE,
  // WriteToPipe writes at most one byte at a time.
  ONE_BYTE_AT_A_TIME,
};

struct DataPipeToSourceStreamTestParam {
  DataPipeToSourceStreamTestParam(uint32_t pipe_capacity,
                                  int buffer_size,
                                  WriteMode write_mode)
      : pipe_capacity(pipe_capacity),
        buffer_size(buffer_size),
        write_mode(write_mode) {}

  const uint32_t pipe_capacity;
  const int buffer_size;
  const WriteMode write_mode;
};

}  // namespace

class DataPipeToSourceStreamTest
    : public ::testing::TestWithParam<DataPipeToSourceStreamTestParam> {
 protected:
  DataPipeToSourceStreamTest()
      : output_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
            GetParam().buffer_size)) {}

  void Init(base::StringPiece message) {
    message_ = message;
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        GetParam().pipe_capacity};
    mojo::ScopedDataPipeConsumerHandle consumer_end;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(&data_pipe_options, &producer_end_,
                                  &consumer_end));
    adapter_ =
        std::make_unique<DataPipeToSourceStream>(std::move(consumer_end));
  }

  // Makes a call to |producer_end_->WriteData| to write the content of
  // |message_| to the data pipe, and closes the producer handle once all data
  // has been written.
  void WriteToPipe() {
    if (message_.empty()) {
      if (producer_end_.is_valid())
        CloseProducerHandle();
      return;
    }
    bool one_byte_at_a_time =
        GetParam().write_mode == WriteMode::ONE_BYTE_AT_A_TIME;
    uint32_t num_bytes = one_byte_at_a_time ? 1 : message_.size();
    MojoResult result = producer_end_->WriteData(message_.data(), &num_bytes,
                                                 MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      message_.remove_prefix(num_bytes);
      if (message_.empty())
        CloseProducerHandle();
      return;
    }
    EXPECT_EQ(result, MOJO_RESULT_SHOULD_WAIT);
  }

  // Reads from |adapter_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns 0 and appends data read to |output|.
  int ReadLoop(std::string* output) {
    while (true) {
      net::TestCompletionCallback callback;
      int rv = adapter_->Read(output_buffer_.get(), output_buffer_->size(),
                              callback.callback());
      if (rv == net::ERR_IO_PENDING) {
        WriteToPipe();
        rv = callback.WaitForResult();
      }
      if (rv == net::OK) {
        EXPECT_FALSE(adapter_->MayHaveMoreBytes());
        break;
      }
      if (rv < net::OK) {
        EXPECT_FALSE(adapter_->MayHaveMoreBytes());
        return rv;
      }
      EXPECT_GT(rv, net::OK);
      output->append(output_buffer_->data(), rv);
      EXPECT_TRUE(adapter_->MayHaveMoreBytes());
    }
    EXPECT_FALSE(adapter_->MayHaveMoreBytes());
    return 0;
  }

  mojo::DataPipeProducerHandle producer_end() { return producer_end_.get(); }
  void CloseProducerHandle() { producer_end_.reset(); }
  void CloseAdapter() { adapter_ = nullptr; }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<net::IOBufferWithSize> output_buffer_;
  std::unique_ptr<DataPipeToSourceStream> adapter_;
  mojo::ScopedDataPipeProducerHandle producer_end_;
  base::StringPiece message_;
};

INSTANTIATE_TEST_SUITE_P(
    DataPipeToSourceStreamTests,
    DataPipeToSourceStreamTest,
    ::testing::Values(
        DataPipeToSourceStreamTestParam(kBigPipeCapacity,
                                        kBigBufferSize,
                                        WriteMode::EVERYTHING_AT_ONCE),
        DataPipeToSourceStreamTestParam(kSmallPipeCapacity,
                                        kBigBufferSize,
                                        WriteMode::EVERYTHING_AT_ONCE),
        DataPipeToSourceStreamTestParam(kBigPipeCapacity,
                                        kSmallBufferSize,
                                        WriteMode::EVERYTHING_AT_ONCE),
        DataPipeToSourceStreamTestParam(kSmallPipeCapacity,
                                        kSmallBufferSize,
                                        WriteMode::EVERYTHING_AT_ONCE),
        DataPipeToSourceStreamTestParam(kBigPipeCapacity,
                                        kBigBufferSize,
                                        WriteMode::ONE_BYTE_AT_A_TIME),
        DataPipeToSourceStreamTestParam(kSmallPipeCapacity,
                                        kBigBufferSize,
                                        WriteMode::ONE_BYTE_AT_A_TIME),
        DataPipeToSourceStreamTestParam(kBigPipeCapacity,
                                        kSmallBufferSize,
                                        WriteMode::ONE_BYTE_AT_A_TIME),
        DataPipeToSourceStreamTestParam(kSmallPipeCapacity,
                                        kSmallBufferSize,
                                        WriteMode::ONE_BYTE_AT_A_TIME)));

TEST_P(DataPipeToSourceStreamTest, EmptyStream) {
  Init("");
  std::string output;
  EXPECT_EQ(ReadLoop(&output), net::OK);
  EXPECT_TRUE(output.empty());
}

TEST_P(DataPipeToSourceStreamTest, Simple) {
  const char message[] = "Hello, world!";
  Init(message);
  std::string output;
  EXPECT_EQ(ReadLoop(&output), net::OK);
  EXPECT_EQ(output, message);
}

TEST_P(DataPipeToSourceStreamTest, DestructorClosesConsumerEnd) {
  const char message[] = "Hello, world!";
  Init(message);
  CloseAdapter();
  uint32_t num_bytes = sizeof(message) - 1;
  MojoResult result =
      producer_end().WriteData(message, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  EXPECT_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
}

}  // namespace network
