// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_data_pipe_connector.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerSyntheticResponseDataPipeConnectorTest
    : public testing::Test {
 public:
  ServiceWorkerSyntheticResponseDataPipeConnectorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest, Basic) {
  const std::string kData = "hello world";
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  mojo::BlockingCopyFromString(kData, source_producer);
  source_producer.reset();

  EXPECT_EQ(kData, ReadDataPipe(std::move(dest_consumer)));
  run_loop.Run();
}

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest, IncrementalRead) {
  const std::string kData = "0123456789abcdefghij";
  const uint32_t kDataPipeCapacity = 5;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeCapacity;

  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  mojo::BlockingCopyFromString(kData, source_producer);
  source_producer.reset();

  // Read data incrementally from dest_consumer.
  std::string read_data;
  while (read_data.size() < kData.size()) {
    base::span<const uint8_t> buffer;
    MojoResult rv =
        dest_consumer->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    read_data.append(base::as_string_view(buffer));
    dest_consumer->EndReadData(buffer.size());
  }

  EXPECT_EQ(kData, read_data);
  run_loop.Run();
}

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest, PeerClosed) {
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  // Close the destination consumer to simulate peer closure.
  dest_consumer.reset();

  // Writing data should now trigger Finish().
  mojo::BlockingCopyFromString("data", source_producer);
  source_producer.reset();

  run_loop.Run();
}

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest,
       DataCompleteDelayed) {
  const std::string kData = "0123456789";
  const uint32_t kDataPipeCapacity = 5;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeCapacity;

  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  // Send data that exceeds capacity.
  mojo::BlockingCopyFromString(kData, source_producer);
  // source_producer.reset() will trigger OnDataComplete() via DataPipeDrainer.
  source_producer.reset();

  // At this point, some data should be in internal_buffer_ and data_complete_
  // should be true.
  EXPECT_EQ(kData, ReadDataPipe(std::move(dest_consumer)));
  run_loop.Run();
}

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest, BufferAppending) {
  const std::string kData1 = "abcde";
  const std::string kData2 = "fghij";
  const uint32_t kDataPipeCapacity = 5;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeCapacity;

  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  // Write first chunk. It should fill the pipe.
  size_t actual_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            source_producer->WriteData(base::as_byte_span(kData1),
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                       actual_written_bytes));

  // Wait for DataPipeDrainer to read it and try writing to dest_producer.
  base::RunLoop().RunUntilIdle();

  // Write second chunk. Since dest_producer is full, it should be appended to
  // internal_buffer_.
  ASSERT_EQ(MOJO_RESULT_OK,
            source_producer->WriteData(base::as_byte_span(kData2),
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                       actual_written_bytes));
  source_producer.reset();

  EXPECT_EQ(kData1 + kData2, ReadDataPipe(std::move(dest_consumer)));
  run_loop.Run();
}

TEST_F(ServiceWorkerSyntheticResponseDataPipeConnectorTest,
       BufferAppendingMultipleChunks) {
  const std::string kData1 = "abcde";
  const std::string kData2 = "fghij";
  const std::string kData3 = "klmno";
  const uint32_t kDataPipeCapacity = 5;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeCapacity;

  mojo::ScopedDataPipeConsumerHandle source_consumer;
  mojo::ScopedDataPipeProducerHandle source_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, source_producer, source_consumer));

  mojo::ScopedDataPipeConsumerHandle dest_consumer;
  mojo::ScopedDataPipeProducerHandle dest_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, dest_producer, dest_consumer));

  base::RunLoop run_loop;
  auto connector =
      std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
          std::move(source_consumer));
  connector->Transfer(std::move(dest_producer), run_loop.QuitClosure());

  // Write first chunk. It should fill the pipe.
  size_t actual_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            source_producer->WriteData(base::as_byte_span(kData1),
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                       actual_written_bytes));

  // Wait for DataPipeDrainer to read it and try writing to dest_producer.
  base::RunLoop().RunUntilIdle();

  // Write second chunk. Since dest_producer is full, it should be appended to
  // internal_buffer_.
  ASSERT_EQ(MOJO_RESULT_OK,
            source_producer->WriteData(base::as_byte_span(kData2),
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                       actual_written_bytes));

  // Wait for DataPipeDrainer to read it and append to internal_buffer_.
  base::RunLoop().RunUntilIdle();

  // Write third chunk. internal_buffer_ is not empty now, so it should be
  // appended to internal_buffer_.
  ASSERT_EQ(MOJO_RESULT_OK,
            source_producer->WriteData(base::as_byte_span(kData3),
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
                                       actual_written_bytes));
  source_producer.reset();

  EXPECT_EQ(kData1 + kData2 + kData3, ReadDataPipe(std::move(dest_consumer)));
  run_loop.Run();
}

}  // namespace content
