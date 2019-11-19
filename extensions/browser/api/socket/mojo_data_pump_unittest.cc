// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/mojo_data_pump.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that if |MojoDataPump::receive_stream_| is not ready, MojoDataPump will
// wait and not error out.
TEST(MojoDataPumpTest, ReceiveStreamNotReady) {
  base::test::TaskEnvironment task_environment;
  mojo::DataPipe receive_pipe;
  mojo::DataPipe send_pipe;
  auto pump =
      std::make_unique<MojoDataPump>(std::move(receive_pipe.consumer_handle),
                                     std::move(send_pipe.producer_handle));
  std::string data("dummy");
  base::RunLoop run_loop;
  bool callback_called = false;
  pump->Read(10 /*count*/,
             base::BindLambdaForTesting(
                 [&](int result, scoped_refptr<net::IOBuffer> io_buffer) {
                   callback_called = true;
                   ASSERT_EQ(static_cast<int>(data.size()), result);
                   EXPECT_EQ(data,
                             std::string(io_buffer->data(), result));
                   run_loop.Quit();
                 }));

  // Spin the message loop so that MojoDataPump::ReceiveMore() is called but the
  // callback will not be executed yet because there is no data to read.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);

  mojo::ScopedDataPipeProducerHandle receive_pipe_producer =
      std::move(receive_pipe.producer_handle);

  uint32_t num_bytes = data.size();
  // WriteData() completes synchronously because |data| is much smaller than
  // data pipe's internal buffer.
  MojoResult r = receive_pipe_producer->WriteData(data.data(), &num_bytes,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, r);
  ASSERT_EQ(data.size(), num_bytes);

  // Now pump->Read() should complete.
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that if |MojoDataPump::receive_stream_| is closed, an error is
// propagated.
TEST(MojoDataPumpTest, ReceiveStreamClosed) {
  base::test::TaskEnvironment task_environment;
  mojo::DataPipe receive_pipe;
  mojo::DataPipe send_pipe;
  auto pump =
      std::make_unique<MojoDataPump>(std::move(receive_pipe.consumer_handle),
                                     std::move(send_pipe.producer_handle));
  base::RunLoop run_loop;
  pump->Read(10 /*count*/,
             base::BindLambdaForTesting(
                 [&](int result, scoped_refptr<net::IOBuffer> io_buffer) {
                   EXPECT_EQ(0, result);
                   EXPECT_EQ(nullptr, io_buffer);
                   run_loop.Quit();
                 }));

  mojo::ScopedDataPipeProducerHandle receive_pipe_producer =
      std::move(receive_pipe.producer_handle);
  receive_pipe_producer.reset();

  // Now pump->Read() should complete.
  run_loop.Run();
}

// Tests that if |MojoDataPump::send_stream_| is closed, Write() will fail.
TEST(MojoDataPumpTest, SendStreamClosed) {
  base::test::TaskEnvironment task_environment;
  mojo::DataPipe receive_pipe;
  mojo::DataPipe send_pipe;
  auto pump =
      std::make_unique<MojoDataPump>(std::move(receive_pipe.consumer_handle),
                                     std::move(send_pipe.producer_handle));
  scoped_refptr<net::StringIOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>("dummy");
  net::TestCompletionCallback callback;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer =
      std::move(send_pipe.consumer_handle);
  send_pipe_consumer.reset();
  pump->Write(write_buffer.get(), write_buffer->size(), callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.WaitForResult());
}

}  // namespace extensions
