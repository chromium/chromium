// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <stddef.h>
#include <stdio.h>

#include <array>
#include <memory>
#include <sstream>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#endif

namespace {

const char kHelloString[] = "Hello, SyncSocket Client";
const size_t kHelloStringLength = std::size(kHelloString);

using SyncSocketTest = ::testing::Test;

// A blocking read operation that will block the thread until it receives
// |buffer|'s length bytes of packets or Shutdown() is called on another thread.
static void BlockingRead(base::SyncSocket* socket,
                         base::span<uint8_t> buffer,
                         size_t* received) {
  // Notify the parent thread that we're up and running.
  socket->Send(base::as_byte_span(kHelloString));
  *received = socket->Receive(buffer);
}

// Tests that we can safely end a blocking Receive operation on one thread
// from another thread by disconnecting (but not closing) the socket.
TEST_F(SyncSocketTest, DisconnectTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[0xff];
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingRead, &pair[0],
                                base::as_writable_byte_span(buf), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));
  EXPECT_EQ(UNSAFE_TODO(strcmp(hello, kHelloString)), 0);
  // Give the worker a chance to start Receive().
  base::PlatformThread::YieldCurrentThread();

  // Now shut down the socket that the thread is issuing a blocking read on
  // which should cause Receive to return with an error.
  pair[0].Shutdown();

  worker.Stop();

  EXPECT_EQ(0U, received);
}

// Tests that read is a blocking operation.
TEST_F(SyncSocketTest, BlockingReceiveTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[kHelloStringLength] = {};
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingRead, &pair[0],
                                base::as_writable_byte_span(buf), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));
  EXPECT_EQ(0, UNSAFE_TODO(strcmp(hello, kHelloString)));
  // Give the worker a chance to start Receive().
  base::PlatformThread::YieldCurrentThread();

  // Send a message to the socket on the blocking thead, it should free the
  // socket from Receive().
  auto bytes_to_send = base::as_byte_span(kHelloString);
  pair[1].Send(bytes_to_send);
  worker.Stop();

  // Verify the socket has received the message.
  EXPECT_TRUE(UNSAFE_TODO(strcmp(buf, kHelloString)) == 0);
  EXPECT_EQ(received, bytes_to_send.size());
}

// Tests that the write operation is non-blocking and returns immediately
// when there is insufficient space in the socket's buffer.
TEST_F(SyncSocketTest, NonBlockingWriteTest) {
  std::array<base::CancelableSyncSocket, 2> pair;
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  // Fill up the buffer for one of the socket, Send() should not block the
  // thread even when the buffer is full.
  auto bytes_to_send = base::as_byte_span(kHelloString);
  while (pair[0].Send(bytes_to_send) != 0) {
  }

  // Data should be avialble on another socket.
  size_t bytes_in_buffer = pair[1].Peek();
  EXPECT_NE(bytes_in_buffer, 0U);

  // No more data can be written to the buffer since socket has been full,
  // verify that the amount of avialble data on another socket is unchanged.
  EXPECT_EQ(pair[0].Send(bytes_to_send), 0U);
  EXPECT_EQ(bytes_in_buffer, pair[1].Peek());

  // Read from another socket to free some space for a new write.
  char hello[kHelloStringLength] = {};
  pair[1].Receive(base::as_writable_byte_span(hello));

  // Should be able to write more data to the buffer now.
  EXPECT_EQ(pair[0].Send(bytes_to_send), bytes_to_send.size());
}

}  // namespace
