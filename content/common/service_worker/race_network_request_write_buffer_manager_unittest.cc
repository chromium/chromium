// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_write_buffer_manager.h"

#include "base/containers/span.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(RaceNetworkRequestWriteBufferManagerTest, WriteData) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  RaceNetworkRequestWriteBufferManager buffer_manager;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(buffer_manager.is_data_pipe_created());

  const char expected_data[] = "abcde";
  base::span<const char> read_buffer = base::make_span(expected_data);
  EXPECT_EQ(buffer_manager.BeginWriteData(), MOJO_RESULT_OK);
  size_t wirtten_num_bytes =
      buffer_manager.CopyAndCompleteWriteData(read_buffer);
  EXPECT_EQ(wirtten_num_bytes, sizeof(expected_data));
}

TEST(RaceNetworkRequestWriteBufferManagerTest, WatchDataPipe) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  RaceNetworkRequestWriteBufferManager buffer_manager;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(buffer_manager.is_data_pipe_created());

  buffer_manager.Watch(base::BindLambdaForTesting(
      [&](MojoResult result, const mojo::HandleSignalsState& state) {
        EXPECT_EQ(result, MOJO_RESULT_OK);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
      }));

  buffer_manager.ArmOrNotify();
  run_loop.Run();
  const char expected_data[] = "abcde";
  base::span<const char> read_buffer = base::make_span(expected_data);
  EXPECT_EQ(buffer_manager.BeginWriteData(), MOJO_RESULT_OK);
  size_t wirtten_num_bytes =
      buffer_manager.CopyAndCompleteWriteData(read_buffer);
  EXPECT_EQ(wirtten_num_bytes, sizeof(expected_data));
}
}  // namespace
}  // namespace content
