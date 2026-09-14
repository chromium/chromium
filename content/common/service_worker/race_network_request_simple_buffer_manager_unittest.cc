// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/common/service_worker/race_network_request_simple_buffer_manager.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class RaceNetworkRequestSimpleBufferManagerTest : public testing::Test {
 public:
  RaceNetworkRequestSimpleBufferManagerTest() = default;
  ~RaceNetworkRequestSimpleBufferManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// This test verifies that the manager can handle multiple OnDataComplete()
// calls between clone operations without crashing. It also confirms that the
// manager successfully completes multiple clone operations.
TEST_F(RaceNetworkRequestSimpleBufferManagerTest,
       HandlesMultipleOnDataCompleteCalls) {
  // Set up the manager.
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  const std::string data = "test data";
  manager->OnDataAvailable(base::as_byte_span(data));

  // Start and complete the first clone operation.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Mark the data drain as complete to allow Finish() to be called.
    manager->OnDataComplete();

    run_loop.Run();

    // Verify the data was written correctly.
    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }

  // Simulate a spurious OnDataComplete() call.
  manager->OnDataComplete();

  // Start and complete a second clone operation to verify the manager has
  // recovered and can tee the data again.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    // Verify the data was written correctly to the second pipe.
    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest, DisconnectDuringClone) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  const std::string data = "test data for disconnect";
  manager->OnDataAvailable(base::as_byte_span(data));
  manager->OnDataComplete();

  // Test 1: Consumer is closed before/during clone.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Close the consumer immediately to simulate early disconnect.
    destination_consumer.reset();

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Verify manager recovered and can handle a subsequent clone cleanly.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest,
       DisconnectBeforeAnyDataReceived) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  // Test 1: Destination disconnects immediately upon Clone() before any network
  // data has arrived.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Disconnect destination consumer immediately.
    destination_consumer.reset();

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Network data arrives after the disconnected clone has completed.
  const std::string data = "test data after disconnect";
  manager->OnDataAvailable(base::as_byte_span(data));
  manager->OnDataComplete();

  // Test 3: Subsequent clone succeeds cleanly.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest,
       DisconnectWhileWaitingForFirstChunk) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  // Test 1: Clone() is initiated with empty buffer, executes initial write
  // attempt, and then destination disconnects while waiting for data.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Ensure the initial OnWriteAvailable task posted during Clone() executes.
    base::RunLoop wait_for_initial_write_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, wait_for_initial_write_loop.QuitClosure());
    wait_for_initial_write_loop.Run();

    // Disconnect destination consumer while waiting for the first chunk.
    destination_consumer.reset();

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Network data arrives after disconnect.
  const std::string data = "test data after idle disconnect";
  manager->OnDataAvailable(base::as_byte_span(data));
  manager->OnDataComplete();

  // Test 3: Subsequent clone succeeds.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest,
       DisconnectDuringStreamingWait) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  const std::string chunk1 = "chunk 1 ";
  manager->OnDataAvailable(base::as_byte_span(chunk1));

  // Test 1: Clone while streaming, chunk1 is written, then destination
  // disconnects while waiting for chunk2.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Wait until chunk1 is available in the consumer pipe before disconnecting.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      size_t num_bytes = 0;
      return destination_consumer->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                            base::span<uint8_t>(),
                                            num_bytes) == MOJO_RESULT_OK &&
             num_bytes == chunk1.size();
    }));

    // Disconnect destination while waiting for chunk2.
    destination_consumer.reset();

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Chunk2 arrives from the network and drain completes.
  const std::string chunk2 = "chunk 2";
  manager->OnDataAvailable(base::as_byte_span(chunk2));
  manager->OnDataComplete();

  // Test 3: Subsequent clone successfully transfers full data (chunk1 +
  // chunk2).
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(chunk1 + chunk2, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest, DisconnectWhileDataPipeFull) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  // Provide data larger than the capacity of the destination data pipe.
  const std::string full_data = "0123456789abcdef";
  manager->OnDataAvailable(base::as_byte_span(full_data));
  manager->OnDataComplete();

  // Test 1: Destination data pipe has capacity 8 bytes, so writing 16 bytes
  // causes MOJO_RESULT_SHOULD_WAIT. Then disconnect consumer.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 8;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, producer, destination_consumer));

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // Wait until the 8-byte pipe is full (SHOULD_WAIT on producer).
    ASSERT_TRUE(base::test::RunUntil([&]() {
      size_t num_bytes = 0;
      return destination_consumer->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                            base::span<uint8_t>(),
                                            num_bytes) == MOJO_RESULT_OK &&
             num_bytes == 8;
    }));

    // Disconnect destination consumer while pipe is full.
    destination_consumer.reset();

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Subsequent clone succeeds and receives all data.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(full_data, received_data);
  }
}

TEST_F(RaceNetworkRequestSimpleBufferManagerTest, DisconnectBeforeClone) {
  mojo::ScopedDataPipeProducerHandle ignored_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, ignored_producer, consumer));
  auto manager = std::make_unique<RaceNetworkRequestSimpleBufferManager>(
      std::move(consumer));

  const std::string data = "test data";
  manager->OnDataAvailable(base::as_byte_span(data));
  manager->OnDataComplete();

  // Test 1: Destination consumer is closed before Clone() is called.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    destination_consumer.reset();

    manager->Clone(std::move(producer), run_loop.QuitClosure());

    // RunLoop should finish without hanging.
    run_loop.Run();
  }

  // Test 2: Subsequent clone succeeds.
  {
    base::RunLoop run_loop;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle destination_consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, destination_consumer));
    manager->Clone(std::move(producer), run_loop.QuitClosure());

    run_loop.Run();

    std::string received_data;
    ASSERT_TRUE(mojo::BlockingCopyToString(std::move(destination_consumer),
                                           &received_data));
    EXPECT_EQ(data, received_data);
  }
}
}  // namespace
}  // namespace content
