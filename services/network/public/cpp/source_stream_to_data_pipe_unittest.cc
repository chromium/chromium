// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/source_stream_to_data_pipe.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const int kBigPipeCapacity = 4096;
const int kSmallPipeCapacity = 1;

enum class ReadResultType {
  // Each call to AddReadResult is a separate read from the lower layer
  // SourceStream. This doesn't work with kSmallPipeCapacity, because
  // MockSourceStream expects that IOBuffer size is not smaller than the data
  // chunk passed to AddReadResult.
  EVERYTHING_AT_ONCE,
  // Whenever AddReadResult is called, each byte is actually a separate read
  // result.
  ONE_BYTE_AT_A_TIME,
};

struct SourceStreamToDataPipeTestParam {
  SourceStreamToDataPipeTestParam(uint32_t pipe_capacity,
                                  net::MockSourceStream::Mode read_mode,
                                  ReadResultType read_result_type)
      : pipe_capacity(pipe_capacity),
        mode(read_mode),
        read_result_type(read_result_type) {}

  const uint32_t pipe_capacity;
  const net::MockSourceStream::Mode mode;
  const ReadResultType read_result_type;
};

}  // namespace

class SourceStreamToDataPipeTest
    : public ::testing::TestWithParam<SourceStreamToDataPipeTestParam> {
 protected:
  SourceStreamToDataPipeTest() {}

  void Init() {
    std::unique_ptr<net::MockSourceStream> source(new net::MockSourceStream());
    if (GetParam().read_result_type == ReadResultType::ONE_BYTE_AT_A_TIME)
      source->set_read_one_byte_at_a_time(true);
    source_ = source.get();

    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        GetParam().pipe_capacity};
    mojo::ScopedDataPipeProducerHandle producer_end;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(&data_pipe_options, &producer_end,
                                  &consumer_end_));

    adapter_ = std::make_unique<SourceStreamToDataPipe>(
        std::move(source), std::move(producer_end));
  }

  base::OnceCallback<void(int)> callback() {
    return base::BindOnce(&SourceStreamToDataPipeTest::FinishedReading,
                          base::Unretained(this));
  }

  void CompleteReadsIfAsync() {
    if (GetParam().mode == net::MockSourceStream::ASYNC) {
      while (source()->awaiting_completion())
        source()->CompleteNextRead();
    }
  }

  // Reads from |consumer_end_| until an error occurs or the producer end is
  // closed. Returns the value passed to the completion callback.
  int ReadPipe(std::string* output) {
    MojoResult result = MOJO_RESULT_OK;
    while (result == MOJO_RESULT_OK || result == MOJO_RESULT_SHOULD_WAIT) {
      char buffer[16];
      uint32_t read_size = sizeof(buffer);
      MojoResult result =
          consumer_end().ReadData(buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_FAILED_PRECONDITION)
        break;
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        RunUntilIdle();
        CompleteReadsIfAsync();
      } else {
        EXPECT_EQ(result, MOJO_RESULT_OK);
        output->append(buffer, read_size);
      }
    }
    EXPECT_TRUE(CallbackResult().has_value());
    return *CallbackResult();
  }

  SourceStreamToDataPipe* adapter() { return adapter_.get(); }
  net::MockSourceStream* source() { return source_; }
  mojo::DataPipeConsumerHandle consumer_end() { return consumer_end_.get(); }

  void CloseConsumerHandle() { consumer_end_.reset(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  base::Optional<int> CallbackResult() { return callback_result_; }

 private:
  void FinishedReading(int result) { callback_result_ = result; }

  base::test::TaskEnvironment task_environment_;
  net::MockSourceStream* source_;
  std::unique_ptr<SourceStreamToDataPipe> adapter_;
  mojo::ScopedDataPipeConsumerHandle consumer_end_;
  base::Optional<int> callback_result_;
};

INSTANTIATE_TEST_SUITE_P(
    SourceStreamToDataPipeTests,
    SourceStreamToDataPipeTest,
    ::testing::Values(
        SourceStreamToDataPipeTestParam(kBigPipeCapacity,
                                        net::MockSourceStream::SYNC,
                                        ReadResultType::EVERYTHING_AT_ONCE),
        SourceStreamToDataPipeTestParam(kBigPipeCapacity,
                                        net::MockSourceStream::ASYNC,
                                        ReadResultType::EVERYTHING_AT_ONCE),
        SourceStreamToDataPipeTestParam(kBigPipeCapacity,
                                        net::MockSourceStream::SYNC,
                                        ReadResultType::ONE_BYTE_AT_A_TIME),
        SourceStreamToDataPipeTestParam(kSmallPipeCapacity,
                                        net::MockSourceStream::SYNC,
                                        ReadResultType::ONE_BYTE_AT_A_TIME),
        SourceStreamToDataPipeTestParam(kBigPipeCapacity,
                                        net::MockSourceStream::ASYNC,
                                        ReadResultType::ONE_BYTE_AT_A_TIME),
        SourceStreamToDataPipeTestParam(kSmallPipeCapacity,
                                        net::MockSourceStream::ASYNC,
                                        ReadResultType::ONE_BYTE_AT_A_TIME)));

TEST_P(SourceStreamToDataPipeTest, EmptyStream) {
  Init();
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  adapter()->Start(callback());

  std::string output;
  EXPECT_EQ(ReadPipe(&output), net::OK);
  EXPECT_TRUE(output.empty());
}

TEST_P(SourceStreamToDataPipeTest, Simple) {
  const char message[] = "Hello, world!";

  Init();
  source()->AddReadResult(message, sizeof(message) - 1, net::OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  adapter()->Start(callback());

  std::string output;
  EXPECT_EQ(ReadPipe(&output), net::OK);
  EXPECT_EQ(output, message);
}

TEST_P(SourceStreamToDataPipeTest, Error) {
  const char message[] = "Hello, world!";

  Init();
  source()->AddReadResult(message, sizeof(message) - 1, net::OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, net::ERR_FAILED, GetParam().mode);
  adapter()->Start(callback());

  std::string output;
  EXPECT_EQ(ReadPipe(&output), net::ERR_FAILED);
  EXPECT_EQ(output, message);
}

TEST_P(SourceStreamToDataPipeTest, ConsumerClosed) {
  const char message[] = "a";

  Init();
  source()->AddReadResult(message, sizeof(message) - 1, net::OK,
                          GetParam().mode);
  adapter()->Start(callback());

  CloseConsumerHandle();
  CompleteReadsIfAsync();
  RunUntilIdle();

  ASSERT_TRUE(CallbackResult().has_value());
  EXPECT_EQ(*CallbackResult(), net::ERR_ABORTED);
}

TEST_P(SourceStreamToDataPipeTest, MayHaveMoreBytes) {
  const char message[] = "Hello, world!";

  // Test that having the SourceStream properly report when !MayHaveMoreBytes
  // shortcuts extra work and still reports things properly.
  Init();
  source()->set_always_report_has_more_bytes(false);
  // Unlike other test reads (see "Simple" test), there is only one result here.
  source()->AddReadResult(message, sizeof(message) - 1, net::OK,
                          GetParam().mode);
  adapter()->Start(callback());

  std::string output;
  EXPECT_EQ(ReadPipe(&output), net::OK);
  EXPECT_EQ(output, message);
}
}  // namespace network
