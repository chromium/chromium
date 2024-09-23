// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_read_buffer_manager.h"
#include <string_view>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(RaceNetworkRequestReadBufferManagerTest, ReadData) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  EXPECT_EQ(mojo::CreateDataPipe(10u, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  mojo::SimpleWatcher producer_watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  RaceNetworkRequestReadBufferManager buffer_manager(
      std::move(consumer_handle));

  const std::string_view expected_data = "abcde";
  size_t actually_written_bytes = 0;
  producer_watcher.Watch(
      producer_handle.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult result, const mojo::HandleSignalsState& state) {
            if (state.writable()) {
              EXPECT_EQ(result, MOJO_RESULT_OK);
              result = producer_handle->WriteData(
                  base::as_byte_span(expected_data), MOJO_WRITE_DATA_FLAG_NONE,
                  actually_written_bytes);
              EXPECT_EQ(result, MOJO_RESULT_OK);
              buffer_manager.ArmOrNotify();
              producer_handle.reset();
            }
          }));

  buffer_manager.Watch(base::BindLambdaForTesting(
      [&](MojoResult result, const mojo::HandleSignalsState& state) {
        EXPECT_EQ(result, MOJO_RESULT_OK);
        // ReadData() operation, it returns the buffer with the whole data.
        std::pair<MojoResult, base::span<const char>> first_result =
            buffer_manager.ReadData();
        EXPECT_EQ(first_result.first, MOJO_RESULT_OK);
        EXPECT_EQ(first_result.second.size(), actually_written_bytes);
        EXPECT_EQ(base::as_string_view(first_result.second), expected_data);

        // Consume data with the partial bytes.
        size_t num_bytes_to_consume = 2;
        buffer_manager.ConsumeData(num_bytes_to_consume);

        // RemainingBuffer() operation, it returns the buffer with the remaining
        // data.
        base::span<const char> remaining_buffer =
            buffer_manager.RemainingBuffer();
        EXPECT_EQ(remaining_buffer.size(),
                  actually_written_bytes - num_bytes_to_consume);
        EXPECT_EQ(base::as_string_view(remaining_buffer),
                  expected_data.substr(num_bytes_to_consume));

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
      }));

  producer_watcher.ArmOrNotify();
  run_loop.Run();
}
}  // namespace
}  // namespace content
