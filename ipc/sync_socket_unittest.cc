// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <sstream>
#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "ipc/ipc_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#endif

// IPC messages for testing ----------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

#define IPC_MESSAGE_START TestMsgStart

// Message class to pass a base::SyncSocket::Handle to another process.  This
// is not as easy as it sounds, because of the differences in transferring
// Windows HANDLEs versus posix file descriptors.
#if BUILDFLAG(IS_WIN)
IPC_MESSAGE_CONTROL1(MsgClassSetHandle, base::SyncSocket::Handle)
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
IPC_MESSAGE_CONTROL1(MsgClassSetHandle, base::FileDescriptor)
#endif

// Message class to pass a response to the server.
IPC_MESSAGE_CONTROL1(MsgClassResponse, std::string)

// Message class to tell the server to shut down.
IPC_MESSAGE_CONTROL0(MsgClassShutdown)

// -----------------------------------------------------------------------------

namespace {

const char kHelloString[] = "Hello, SyncSocket Client";
const size_t kHelloStringLength = std::size(kHelloString);

// The SyncSocket server listener class processes two sorts of
// messages from the client.
class SyncSocketServerListener : public IPC::Listener {
 public:
  SyncSocketServerListener() : chan_(nullptr) {}

  SyncSocketServerListener(const SyncSocketServerListener&) = delete;
  SyncSocketServerListener& operator=(const SyncSocketServerListener&) = delete;

  void Init(IPC::Channel* chan) {
    chan_ = chan;
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() == MSG_ROUTING_CONTROL) {
      IPC_BEGIN_MESSAGE_MAP(SyncSocketServerListener, msg)
        IPC_MESSAGE_HANDLER(MsgClassSetHandle, OnMsgClassSetHandle)
        IPC_MESSAGE_HANDLER(MsgClassShutdown, OnMsgClassShutdown)
      IPC_END_MESSAGE_MAP()
    }
    return true;
  }
  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  // This sort of message is sent first, causing the transfer of
  // the handle for the SyncSocket.  This message sends a buffer
  // on the SyncSocket and then sends a response to the client.
#if BUILDFLAG(IS_WIN)
  void OnMsgClassSetHandle(const base::SyncSocket::Handle handle) {
    SetHandle(handle);
  }
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  void OnMsgClassSetHandle(const base::FileDescriptor& fd_struct) {
    SetHandle(fd_struct.fd);
  }
#else
# error "What platform?"
#endif  // BUILDFLAG(IS_WIN)

  void SetHandle(base::SyncSocket::Handle handle) {
    base::SyncSocket sync_socket(handle);
    auto bytes_to_send = base::as_byte_span(kHelloString);
    EXPECT_EQ(sync_socket.Send(bytes_to_send), bytes_to_send.size());
    IPC::Message* msg = new MsgClassResponse(kHelloString);
    EXPECT_TRUE(chan_->Send(msg));
  }

  // When the client responds, it sends back a shutdown message,
  // which causes the message loop to exit.
  void OnMsgClassShutdown() { std::move(quit_closure_).Run(); }

  raw_ptr<IPC::Channel> chan_;
  base::OnceClosure quit_closure_;
};

// Runs the fuzzing server child mode. Returns when the preset number of
// messages have been received.
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(SyncSocketServerClient) {
  SyncSocketServerListener listener;
  base::RunLoop loop;
  listener.set_quit_closure(loop.QuitWhenIdleClosure());
  Connect(&listener);
  listener.Init(channel());
  loop.Run();
  Close();
}

// The SyncSocket client listener only processes one sort of message,
// a response from the server.
class SyncSocketClientListener : public IPC::Listener {
 public:
  SyncSocketClientListener() = default;

  SyncSocketClientListener(const SyncSocketClientListener&) = delete;
  SyncSocketClientListener& operator=(const SyncSocketClientListener&) = delete;

  void Init(base::SyncSocket* socket, IPC::Channel* chan) {
    socket_ = socket;
    chan_ = chan;
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() == MSG_ROUTING_CONTROL) {
      IPC_BEGIN_MESSAGE_MAP(SyncSocketClientListener, msg)
        IPC_MESSAGE_HANDLER(MsgClassResponse, OnMsgClassResponse)
      IPC_END_MESSAGE_MAP()
    }
    return true;
  }
  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  // When a response is received from the server, it sends the same
  // string as was written on the SyncSocket.  These are compared
  // and a shutdown message is sent back to the server.
  void OnMsgClassResponse(const std::string& str) {
    // Account for the terminating null byte.
    size_t expected_bytes_to_receive = str.length() + 1;
    // We rely on the order of sync_socket.Send() and chan_->Send() in
    // the SyncSocketServerListener object.
    EXPECT_EQ(socket_->Peek(), expected_bytes_to_receive);
    base::FixedArray<char> buf(expected_bytes_to_receive);
    socket_->Receive(base::as_writable_bytes(base::make_span(buf)));
    EXPECT_EQ(strcmp(str.c_str(), buf.data()), 0);
    // After receiving from the socket there should be no bytes left.
    EXPECT_EQ(0U, socket_->Peek());
    IPC::Message* msg = new MsgClassShutdown();
    EXPECT_TRUE(chan_->Send(msg));
    std::move(quit_closure_).Run();
  }

  raw_ptr<base::SyncSocket> socket_;
  raw_ptr<IPC::Channel, DanglingUntriaged> chan_;
  base::OnceClosure quit_closure_;
};

using SyncSocketTest = IPCChannelMojoTestBase;

TEST_F(SyncSocketTest, SanityTest) {
  Init("SyncSocketServerClient");
  base::RunLoop loop;

  SyncSocketClientListener listener;
  listener.set_quit_closure(loop.QuitWhenIdleClosure());
  CreateChannel(&listener);
  // Create a pair of SyncSockets.
  base::SyncSocket pair[2];
  base::SyncSocket::CreatePair(&pair[0], &pair[1]);
  // Immediately after creation there should be no pending bytes.
  EXPECT_EQ(0U, pair[0].Peek());
  EXPECT_EQ(0U, pair[1].Peek());
  base::SyncSocket::Handle target_handle;
  // Connect the channel and listener.
  ASSERT_TRUE(ConnectChannel());
  listener.Init(&pair[0], channel());
#if BUILDFLAG(IS_WIN)
  // On windows we need to duplicate the handle into the server process.
  BOOL retval = DuplicateHandle(GetCurrentProcess(), pair[1].handle(),
                                client_process().Handle(), &target_handle,
                                0, FALSE, DUPLICATE_SAME_ACCESS);
  EXPECT_TRUE(retval);
  // Set up a message to pass the handle to the server.
  IPC::Message* msg = new MsgClassSetHandle(target_handle);
#else
  target_handle = pair[1].handle();
  // Set up a message to pass the handle to the server.
  base::FileDescriptor filedesc(target_handle, false);
  IPC::Message* msg = new MsgClassSetHandle(filedesc);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_TRUE(sender()->Send(msg));
  // Use the current thread as the I/O thread.
  loop.Run();
  // Shut down.
  pair[0].Close();
  pair[1].Close();
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

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
  base::CancelableSyncSocket pair[2];
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[0xff];
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BlockingRead, &pair[0],
                     base::as_writable_bytes(base::make_span(buf)), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {0};
  pair[1].Receive(base::as_writable_bytes(base::make_span(hello)));
  EXPECT_EQ(strcmp(hello, kHelloString), 0);
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
  base::CancelableSyncSocket pair[2];
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  base::Thread worker("BlockingThread");
  worker.Start();

  // Try to do a blocking read from one of the sockets on the worker thread.
  char buf[kHelloStringLength] = {0};
  size_t received = 1U;  // Initialize to an unexpected value.
  worker.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BlockingRead, &pair[0],
                     base::as_writable_bytes(base::make_span(buf)), &received));

  // Wait for the worker thread to say hello.
  char hello[kHelloStringLength] = {0};
  pair[1].Receive(base::as_writable_bytes(base::make_span(hello)));
  EXPECT_EQ(0, strcmp(hello, kHelloString));
  // Give the worker a chance to start Receive().
  base::PlatformThread::YieldCurrentThread();

  // Send a message to the socket on the blocking thead, it should free the
  // socket from Receive().
  auto bytes_to_send = base::as_byte_span(kHelloString);
  pair[1].Send(bytes_to_send);
  worker.Stop();

  // Verify the socket has received the message.
  EXPECT_TRUE(strcmp(buf, kHelloString) == 0);
  EXPECT_EQ(received, bytes_to_send.size());
}

// Tests that the write operation is non-blocking and returns immediately
// when there is insufficient space in the socket's buffer.
TEST_F(SyncSocketTest, NonBlockingWriteTest) {
  base::CancelableSyncSocket pair[2];
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
  char hello[kHelloStringLength] = {0};
  pair[1].Receive(base::as_writable_bytes(base::make_span(hello)));

  // Should be able to write more data to the buffer now.
  EXPECT_EQ(pair[0].Send(bytes_to_send), bytes_to_send.size());
}

}  // namespace
