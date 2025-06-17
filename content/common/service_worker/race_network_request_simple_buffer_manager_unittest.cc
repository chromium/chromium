// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/common/service_worker/race_network_request_simple_buffer_manager.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
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
}  // namespace
}  // namespace content
