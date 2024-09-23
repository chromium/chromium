// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/mojo_data_pump.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that if |MojoDataPump::receive_stream_| is not ready, MojoDataPump will
// wait and not error out.
TEST(MojoDataPumpTest, ReceiveStreamNotReady) {
  base::test::TaskEnvironment task_environment;

  mojo::ScopedDataPipeProducerHandle receive_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, receive_producer_handle,
                                 receive_consumer_handle),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle send_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
  ASSERT_EQ(
      mojo::CreateDataPipe(nullptr, send_producer_handle, send_consumer_handle),
      MOJO_RESULT_OK);

  auto pump = std::make_unique<MojoDataPump>(std::move(receive_consumer_handle),
                                             std::move(send_producer_handle));
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

  // WriteData() completes synchronously because |data| is much smaller than
  // data pipe's internal buffer.
  size_t actually_written_bytes = 0;
  MojoResult r = receive_producer_handle->WriteData(base::as_byte_span(data),
                                                    MOJO_WRITE_DATA_FLAG_NONE,
                                                    actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, r);
  ASSERT_EQ(data.size(), actually_written_bytes);

  // Now pump->Read() should complete.
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that if |MojoDataPump::receive_stream_| is closed, an error is
// propagated.
TEST(MojoDataPumpTest, ReceiveStreamClosed) {
  base::test::TaskEnvironment task_environment;
  mojo::ScopedDataPipeProducerHandle receive_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, receive_producer_handle,
                                 receive_consumer_handle),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle send_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
  ASSERT_EQ(
      mojo::CreateDataPipe(nullptr, send_producer_handle, send_consumer_handle),
      MOJO_RESULT_OK);

  auto pump = std::make_unique<MojoDataPump>(std::move(receive_consumer_handle),
                                             std::move(send_producer_handle));
  base::RunLoop run_loop;
  pump->Read(10 /*count*/,
             base::BindLambdaForTesting(
                 [&](int result, scoped_refptr<net::IOBuffer> io_buffer) {
                   EXPECT_EQ(0, result);
                   EXPECT_EQ(nullptr, io_buffer);
                   run_loop.Quit();
                 }));

  receive_producer_handle.reset();

  // Now pump->Read() should complete.
  run_loop.Run();
}

// Tests that if |MojoDataPump::send_stream_| is closed, Write() will fail.
TEST(MojoDataPumpTest, SendStreamClosed) {
  base::test::TaskEnvironment task_environment;
  mojo::ScopedDataPipeProducerHandle receive_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, receive_producer_handle,
                                 receive_consumer_handle),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle send_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
  ASSERT_EQ(
      mojo::CreateDataPipe(nullptr, send_producer_handle, send_consumer_handle),
      MOJO_RESULT_OK);

  auto pump = std::make_unique<MojoDataPump>(std::move(receive_consumer_handle),
                                             std::move(send_producer_handle));
  scoped_refptr<net::StringIOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>("dummy");
  net::TestCompletionCallback callback;
  send_consumer_handle.reset();
  pump->Write(write_buffer.get(), write_buffer->size(), callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.WaitForResult());
}

}  // namespace extensions
