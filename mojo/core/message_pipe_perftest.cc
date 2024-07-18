// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_time_logger.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/test_utils.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

class MessagePipePerfTest : public test::MojoTestBase {
 public:
  MessagePipePerfTest() : message_count_(0), message_size_(0) {}

  MessagePipePerfTest(const MessagePipePerfTest&) = delete;
  MessagePipePerfTest& operator=(const MessagePipePerfTest&) = delete;

  void SetUpMeasurement(int message_count, size_t message_size) {
    message_count_ = message_count;
    message_size_ = message_size;
    payload_ = std::string(message_size, '*');
    read_buffer_.resize(message_size * 2);
  }

 protected:
  void WriteWaitThenRead(MojoHandle mp) {
    CHECK_EQ(
        WriteMessageRaw(MessagePipeHandle(mp), payload_.data(), payload_.size(),
                        nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE),
        MOJO_RESULT_OK);
    HandleSignalsState hss;
    CHECK_EQ(WaitForSignals(mp, MOJO_HANDLE_SIGNAL_READABLE, &hss),
             MOJO_RESULT_OK);
    CHECK_EQ(ReadMessageRaw(MessagePipeHandle(mp), &read_buffer_, nullptr,
                            MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(read_buffer_.size(), payload_.size());
  }

  void SendQuitMessage(MojoHandle mp) {
    CHECK_EQ(WriteMessageRaw(MessagePipeHandle(mp), "", 0, nullptr, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

  void Measure(MojoHandle mp) {
    // Have one ping-pong to ensure channel being established.
    WriteWaitThenRead(mp);

    std::string test_name =
        base::StringPrintf("IPC_Perf_%dx_%u", message_count_,
                           static_cast<unsigned>(message_size_));
    base::PerfTimeLogger logger(test_name.c_str());

    for (int i = 0; i < message_count_; ++i)
      WriteWaitThenRead(mp);

    logger.Done();
  }

 protected:
  void RunPingPongServer(MojoHandle mp) {
    // This values are set to align with one at ipc_pertests.cc for comparison.
    const size_t kMsgSize[5] = {12, 144, 1728, 20736, 248832};
    const int kMessageCount[5] = {50000, 50000, 50000, 12000, 1000};

    for (size_t i = 0; i < 5; i++) {
      SetUpMeasurement(kMessageCount[i], kMsgSize[i]);
      Measure(mp);
    }

    SendQuitMessage(mp);
  }

  static int RunPingPongClient(MojoHandle mp) {
    std::vector<uint8_t> buffer;
    int rv = 0;
    while (true) {
      // Wait for our end of the message pipe to be readable.
      HandleSignalsState hss;
      MojoResult result = WaitForSignals(mp, MOJO_HANDLE_SIGNAL_READABLE, &hss);
      if (result != MOJO_RESULT_OK) {
        rv = result;
        break;
      }

      CHECK_EQ(ReadMessageRaw(MessagePipeHandle(mp), &buffer, nullptr,
                              MOJO_READ_MESSAGE_FLAG_NONE),
               MOJO_RESULT_OK);

      // Empty message indicates quit.
      if (buffer.empty())
        break;

      CHECK_EQ(
          WriteMessageRaw(MessagePipeHandle(mp), buffer.data(), buffer.size(),
                          nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE),
          MOJO_RESULT_OK);
    }

    return rv;
  }

 private:
  int message_count_;
  size_t message_size_;
  std::string payload_;
  std::vector<uint8_t> read_buffer_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;
};

TEST_F(MessagePipePerfTest, PingPong) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::Thread client_thread("PingPongClient");
  client_thread.Start();
  client_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&RunPingPongClient), client_handle));

  RunPingPongServer(server_handle);
}

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
DEFINE_TEST_CLIENT_WITH_PIPE(PingPongClient, MessagePipePerfTest, h) {
  return RunPingPongClient(h);
}

// Repeatedly sends messages as previous one got replied by the child.
// Waits for the child to close its end before quitting once specified
// number of messages has been sent.
TEST_F(MessagePipePerfTest, MultiprocessPingPong) {
  RunTestClient("PingPongClient", [&](MojoHandle h) { RunPingPongServer(h); });
}

}  // namespace
}  // namespace core
}  // namespace mojo
