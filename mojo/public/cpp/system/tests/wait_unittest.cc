// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/wait.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using WaitTest = testing::Test;
using WaitManyTest = testing::Test;

void WriteMessage(const ScopedMessagePipeHandle& handle,
                  const std::string_view& message) {
  MojoResult rv = WriteMessageRaw(handle.get(), message.data(),
                                  static_cast<uint32_t>(message.size()),
                                  nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  CHECK_EQ(MOJO_RESULT_OK, rv);
}

std::string ReadMessage(const ScopedMessagePipeHandle& handle) {
  std::vector<uint8_t> bytes;
  MojoResult rv = ReadMessageRaw(handle.get(), &bytes, nullptr,
                                 MOJO_READ_MESSAGE_FLAG_NONE);
  CHECK_EQ(MOJO_RESULT_OK, rv);
  return std::string(bytes.begin(), bytes.end());
}

class ThreadedRunner : public base::SimpleThread {
 public:
  explicit ThreadedRunner(base::OnceClosure callback)
      : SimpleThread("ThreadedRunner"), callback_(std::move(callback)) {}

  ThreadedRunner(const ThreadedRunner&) = delete;
  ThreadedRunner& operator=(const ThreadedRunner&) = delete;

  ~ThreadedRunner() override { Join(); }

  void Run() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
};

TEST_F(WaitTest, InvalidArguments) {
  Handle invalid_handle;

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            Wait(invalid_handle, MOJO_HANDLE_SIGNAL_READABLE));

  MessagePipe p;
  Handle valid_handles[2] = {p.handle0.get(), p.handle1.get()};
  Handle invalid_handles[2];
  MojoHandleSignals signals[2] = {MOJO_HANDLE_SIGNAL_NONE,
                                  MOJO_HANDLE_SIGNAL_NONE};
  size_t result_index = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            WaitMany(invalid_handles, signals, 2, &result_index));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            WaitMany(nullptr, signals, 2, &result_index));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            WaitMany(valid_handles, nullptr, 2, &result_index));
}

TEST_F(WaitTest, Basic) {
  MessagePipe p;

  // Write to one end of the pipe and wait on the other.
  const char kTestMessage1[] = "how about a nice game of chess?";
  WriteMessage(p.handle0, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK, Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE));

  // And make sure we can also grab the handle signals state (with both the C
  // and C++ library structs.)

  MojoHandleSignalsState c_hss = {0, 0};
  EXPECT_EQ(MOJO_RESULT_OK,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &c_hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            c_hss.satisfied_signals);

  HandleSignalsState hss;
  EXPECT_EQ(MOJO_RESULT_OK,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.readable() && hss.writable() && !hss.peer_closed());
  EXPECT_FALSE(hss.never_readable() || hss.never_writable() ||
               hss.never_peer_closed());

  // Now close the writing end and wait for peer closure.

  p.handle0.reset();
  EXPECT_EQ(MOJO_RESULT_OK,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));

  // Still readable as there's still a message queued. No longer writable as
  // peer closure has been detected.
  EXPECT_TRUE(hss.readable() && hss.peer_closed() && !hss.writable());
  EXPECT_TRUE(hss.never_writable() && !hss.never_readable() &&
              !hss.never_peer_closed());

  // Read the message and wait for readable again. Waiting should fail since
  // there are no more messages and the peer is closed.
  EXPECT_EQ(kTestMessage1, ReadMessage(p.handle1));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &hss));

  // Sanity check the signals state again.
  EXPECT_TRUE(hss.peer_closed() && !hss.readable() && !hss.writable());
  EXPECT_TRUE(hss.never_readable() && hss.never_writable() &&
              !hss.never_peer_closed());
}

TEST_F(WaitTest, DelayedWrite) {
  MessagePipe p;

  ThreadedRunner write_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then write a message.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        WriteMessage(*handle, "wakey wakey");
      },
      &p.handle0));
  write_after_delay.Start();

  HandleSignalsState hss;
  EXPECT_EQ(MOJO_RESULT_OK,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.readable() && hss.writable() && !hss.peer_closed());
  EXPECT_TRUE(!hss.never_readable() && !hss.never_writable() &&
              !hss.never_peer_closed());
}

TEST_F(WaitTest, DelayedPeerClosure) {
  MessagePipe p;

  ThreadedRunner close_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then close the handle.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        handle->reset();
      },
      &p.handle0));
  close_after_delay.Start();

  HandleSignalsState hss;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            Wait(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(!hss.readable() && !hss.writable() && hss.peer_closed());
  EXPECT_TRUE(hss.never_readable() && hss.never_writable() &&
              !hss.never_peer_closed());
}

TEST_F(WaitTest, CloseWhileWaiting) {
  MessagePipe p;
  ThreadedRunner close_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        base::PlatformThread::Sleep(base::Milliseconds(200));
        handle->reset();
      },
      &p.handle0));
  close_after_delay.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED,
            Wait(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE));
}

TEST_F(WaitManyTest, Basic) {
  MessagePipe p;

  const char kTestMessage1[] = "hello";
  WriteMessage(p.handle0, kTestMessage1);

  // Wait for either handle to become readable. Wait twice, just to verify that
  // we can use either the C or C++ signaling state structure for the last
  // argument.

  Handle handles[2] = {p.handle0.get(), p.handle1.get()};
  MojoHandleSignals signals[2] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};
  size_t result_index = 0;
  MojoHandleSignalsState c_hss[2];
  HandleSignalsState hss[2];

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitMany(handles, signals, 2, &result_index, c_hss));
  EXPECT_EQ(1u, result_index);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, c_hss[0].satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            c_hss[1].satisfied_signals);

  EXPECT_EQ(MOJO_RESULT_OK, WaitMany(handles, signals, 2, &result_index, hss));
  EXPECT_EQ(1u, result_index);
  EXPECT_TRUE(!hss[0].readable() && hss[0].writable() && !hss[0].peer_closed());
  EXPECT_TRUE(!hss[0].never_readable() && !hss[0].never_writable() &&
              !hss[0].never_peer_closed());
  EXPECT_TRUE(hss[1].readable() && hss[1].writable() && !hss[1].peer_closed());
  EXPECT_TRUE(!hss[1].never_readable() && !hss[1].never_writable() &&
              !hss[1].never_peer_closed());

  // Close the writer and read the message. Try to wait again, and it should
  // fail due to the conditions being unsatisfiable.

  EXPECT_EQ(kTestMessage1, ReadMessage(p.handle1));
  p.handle0.reset();

  handles[0] = handles[1];
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitMany(handles, signals, 1, &result_index, hss));
  EXPECT_EQ(0u, result_index);
  EXPECT_TRUE(!hss[0].readable() && !hss[0].writable() && hss[0].peer_closed());
  EXPECT_TRUE(hss[0].never_readable() && hss[0].never_writable() &&
              !hss[0].never_peer_closed());
}

TEST_F(WaitManyTest, CloseWhileWaiting) {
  MessagePipe p, q;

  Handle handles[3] = {q.handle0.get(), q.handle1.get(), p.handle1.get()};
  MojoHandleSignals signals[3] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};

  ThreadedRunner close_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        base::PlatformThread::Sleep(base::Milliseconds(200));
        handle->reset();
      },
      &p.handle1));
  close_after_delay.Start();

  size_t result_index = 0;
  EXPECT_EQ(MOJO_RESULT_CANCELLED,
            WaitMany(handles, signals, 3, &result_index));
  EXPECT_EQ(2u, result_index);
}

TEST_F(WaitManyTest, DelayedWrite) {
  MessagePipe p;

  ThreadedRunner write_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then write a message.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        WriteMessage(*handle, "wakey wakey");
      },
      &p.handle0));
  write_after_delay.Start();

  Handle handles[2] = {p.handle0.get(), p.handle1.get()};
  MojoHandleSignals signals[2] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};
  size_t result_index = 0;
  HandleSignalsState hss[2];
  EXPECT_EQ(MOJO_RESULT_OK, WaitMany(handles, signals, 2, &result_index, hss));
  EXPECT_EQ(1u, result_index);
  EXPECT_TRUE(!hss[0].readable() && hss[0].writable() && !hss[0].peer_closed());
  EXPECT_TRUE(!hss[0].never_readable() && !hss[0].never_writable() &&
              !hss[0].never_peer_closed());
  EXPECT_TRUE(hss[1].readable() && hss[1].writable() && !hss[1].peer_closed());
  EXPECT_TRUE(!hss[1].never_readable() && !hss[1].never_writable() &&
              !hss[1].never_peer_closed());
}

TEST_F(WaitManyTest, DelayedPeerClosure) {
  MessagePipe p, q;

  ThreadedRunner close_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then close the handle.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        handle->reset();
      },
      &p.handle0));
  close_after_delay.Start();

  Handle handles[3] = {q.handle0.get(), q.handle1.get(), p.handle1.get()};
  MojoHandleSignals signals[3] = {MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE,
                                  MOJO_HANDLE_SIGNAL_READABLE};
  size_t result_index = 0;
  HandleSignalsState hss[3];
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitMany(handles, signals, 3, &result_index, hss));
  EXPECT_EQ(2u, result_index);
  EXPECT_TRUE(!hss[2].readable() && !hss[2].writable() && hss[2].peer_closed());
  EXPECT_TRUE(hss[2].never_readable() && hss[2].never_writable() &&
              !hss[2].never_peer_closed());
}

}  // namespace
}  // namespace mojo
