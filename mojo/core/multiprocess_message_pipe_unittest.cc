// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/test_utils.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

// Temporary helpers to avoid tons of churn as old APIs are removed. These
// support only enough of a subset of the old APIs to satisfy the usage in these
// tests.
//
// TODO(rockot): Remove these.
MojoResult MojoReadMessage(MojoHandle pipe,
                           void* out_bytes,
                           uint32_t* num_bytes,
                           MojoHandle* out_handles,
                           uint32_t* num_handles,
                           MojoReadMessageFlags flags) {
  std::vector<uint8_t> bytes;
  std::vector<ScopedHandle> handles;
  MojoResult rv =
      ReadMessageRaw(MessagePipeHandle(pipe), &bytes, &handles, flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  if (num_bytes)
    *num_bytes = static_cast<uint32_t>(bytes.size());
  if (!bytes.empty()) {
    CHECK(out_bytes && num_bytes && *num_bytes >= bytes.size());
    memcpy(out_bytes, bytes.data(), bytes.size());
  }

  if (num_handles)
    *num_handles = static_cast<uint32_t>(handles.size());
  if (!handles.empty()) {
    CHECK(out_handles && num_handles && *num_handles >= handles.size());
    for (size_t i = 0; i < handles.size(); ++i)
      out_handles[i] = handles[i].release().value();
  }
  return MOJO_RESULT_OK;
}

MojoResult MojoWriteMessage(MojoHandle pipe,
                            const void* bytes,
                            uint32_t num_bytes,
                            const MojoHandle* handles,
                            uint32_t num_handles,
                            MojoWriteMessageFlags flags) {
  return WriteMessageRaw(MessagePipeHandle(pipe), bytes, num_bytes, handles,
                         num_handles, flags);
}

class MultiprocessMessagePipeTest : public test::MojoTestBase {
 protected:
  // Convenience class for tests which will control command-driven children.
  // See the CommandDrivenClient definition below.
  class CommandDrivenClientController {
   public:
    explicit CommandDrivenClientController(MojoHandle h) : h_(h) {}

    void Send(const std::string& command) {
      WriteMessage(h_, command);
      EXPECT_EQ("ok", ReadMessage(h_));
    }

    void SendHandle(const std::string& name, MojoHandle p) {
      WriteMessageWithHandles(h_, "take:" + name, &p, 1);
      EXPECT_EQ("ok", ReadMessage(h_));
    }

    MojoHandle RetrieveHandle(const std::string& name) {
      WriteMessage(h_, "return:" + name);
      MojoHandle p;
      EXPECT_EQ("ok", ReadMessageWithHandles(h_, &p, 1));
      return p;
    }

    void Exit() { WriteMessage(h_, "exit"); }

   private:
    MojoHandle h_;
  };
};

class MultiprocessMessagePipeTestWithPeerSupport
    : public MultiprocessMessagePipeTest,
      public testing::WithParamInterface<test::MojoTestBase::LaunchType> {
 protected:
  void SetUp() override {
    test::MojoTestBase::SetUp();
    set_launch_type(GetParam());

    const bool is_peer_launch =
        GetParam() == test::MojoTestBase::LaunchType::PEER;
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
    const bool is_named_peer_launch = false;
#else
    const bool is_named_peer_launch =
        GetParam() == test::MojoTestBase::LaunchType::NAMED_PEER;
#endif
    if (is_peer_launch || is_named_peer_launch) {
      GTEST_SKIP() << "Skipping peer connection tests because mojo-ipcz does "
                   << "not yet implement isolated connections.";
    }
  }
};

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoEcho, MultiprocessMessagePipeTest, h) {
  const std::string quitquitquit("quitquitquit");
  int rv = 0;
  for (;; rv = (rv + 1) % 100) {
    // Wait for our end of the message pipe to be readable.
    HandleSignalsState hss;
    MojoResult result = WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss);
    if (result != MOJO_RESULT_OK) {
      // It was closed, probably.
      CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
      CHECK_EQ(hss.satisfied_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      break;
    } else {
      CHECK((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
      CHECK((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));
    }

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &read_buffer_size, nullptr, 0,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Child got: " << read_buffer;

    if (read_buffer == quitquitquit) {
      VLOG(2) << "Child quitting.";
      break;
    }

    std::string write_buffer = read_buffer + read_buffer;
    CHECK_EQ(MojoWriteMessage(h, write_buffer.data(),
                              static_cast<uint32_t>(write_buffer.size()),
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

  return rv;
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, Basic) {
  int exit_code = RunTestClientAndGetExitCode("EchoEcho", [&](MojoHandle h) {
    std::string hello("hello");
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(h, hello.data(), static_cast<uint32_t>(hello.size()),
                         nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));

    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    // The child may or may not have closed its end of the message pipe and died
    // (and we may or may not know it yet), so our end may or may not appear as
    // writable.
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &read_buffer_size, nullptr, 0,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Parent got: " << read_buffer;
    ASSERT_EQ(hello + hello, read_buffer);

    std::string quitquitquit("quitquitquit");
    CHECK_EQ(MojoWriteMessage(h, quitquitquit.data(),
                              static_cast<uint32_t>(quitquitquit.size()),
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  });
  EXPECT_EQ(1, exit_code);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, QueueMessages) {
  static const size_t kNumMessages = 1001;
  int exit_code = RunTestClientAndGetExitCode("EchoEcho", [&](MojoHandle h) {
    for (size_t i = 0; i < kNumMessages; i++) {
      std::string write_buffer(i, 'A' + (i % 26));
      ASSERT_EQ(MOJO_RESULT_OK,
                MojoWriteMessage(h, write_buffer.data(),
                                 static_cast<uint32_t>(write_buffer.size()),
                                 nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));
    }

    for (size_t i = 0; i < kNumMessages; i++) {
      HandleSignalsState hss;
      ASSERT_EQ(MOJO_RESULT_OK,
                WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
      // The child may or may not have closed its end of the message pipe and
      // died (and we may or may not know it yet), so our end may or may not
      // appear as writable.
      ASSERT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
      ASSERT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

      std::string read_buffer(kNumMessages * 2, '\0');
      uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
      ASSERT_EQ(MojoReadMessage(h, &read_buffer[0], &read_buffer_size, nullptr,
                                0, MOJO_READ_MESSAGE_FLAG_NONE),
                MOJO_RESULT_OK);
      read_buffer.resize(read_buffer_size);

      ASSERT_EQ(std::string(i * 2, 'A' + (i % 26)), read_buffer);
    }

    const std::string quitquitquit("quitquitquit");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, quitquitquit.data(),
                               static_cast<uint32_t>(quitquitquit.size()),
                               nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for it to become readable, which should fail (since we sent
    // "quitquitquit").
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    ASSERT_FALSE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
    ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  });
  EXPECT_EQ(static_cast<int>(kNumMessages % 100), exit_code);
}

DEFINE_TEST_CLIENT_WITH_PIPE(CheckSharedBuffer,
                             MultiprocessMessagePipeTest,
                             h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  // It should have a shared buffer.
  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[10];
  uint32_t num_handlers = std::size(handles);  // Maximum number to receive
  CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 1"));
  CHECK_EQ(num_handlers, 1u);

  // Make a mapping.
  void* buffer;
  CHECK_EQ(MojoMapBuffer(handles[0], 0, 100, nullptr, &buffer), MOJO_RESULT_OK);

  // Write some stuff to the shared buffer.
  static const char kHello[] = "hello";
  memcpy(buffer, kHello, sizeof(kHello));

  // We should be able to close the dispatcher now.
  MojoClose(handles[0]);

  // And send a message to signal that we've written stuff.
  const std::string go2("go 2");
  CHECK_EQ(MojoWriteMessage(h, go2.data(), static_cast<uint32_t>(go2.size()),
                            nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  // Now wait for our parent to send us a message.
  hss = HandleSignalsState();
  CHECK_EQ(WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  read_buffer = std::string(100, '\0');
  num_bytes = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &num_bytes, nullptr, 0,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 3"));

  // It should have written something to the shared buffer.
  static const char kWorld[] = "world!!!";
  CHECK_EQ(memcmp(buffer, kWorld, sizeof(kWorld)), 0);

  // And we're done.

  return 0;
}

TEST_F(MultiprocessMessagePipeTest, SharedBufferPassing) {
  RunTestClient("CheckSharedBuffer", [&](MojoHandle h) {
    // Make a shared buffer.
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_FLAG_NONE;

    MojoHandle shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateSharedBuffer(100, &options, &shared_buffer));
    MojoSharedBufferInfo buffer_info;
    buffer_info.struct_size = sizeof(buffer_info);
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoGetBufferInfo(shared_buffer, nullptr, &buffer_info));
    EXPECT_GE(buffer_info.size, 100U);

    // Send the shared buffer.
    const std::string go1("go 1");

    MojoHandle duplicated_shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoDuplicateBufferHandle(shared_buffer, nullptr,
                                        &duplicated_shared_buffer));
    buffer_info.size = 0;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoGetBufferInfo(shared_buffer, nullptr, &buffer_info));
    EXPECT_GE(buffer_info.size, 100U);
    MojoHandle handles[1];
    handles[0] = duplicated_shared_buffer;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, &go1[0], static_cast<uint32_t>(go1.size()),
                               &handles[0], std::size(handles),
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoReadMessage(h, &read_buffer[0], &num_bytes, nullptr, 0,
                              MOJO_READ_MESSAGE_FLAG_NONE));
    read_buffer.resize(num_bytes);
    ASSERT_EQ(std::string("go 2"), read_buffer);

    // After we get it, the child should have written something to the shared
    // buffer.
    static const char kHello[] = "hello";
    void* buffer;
    CHECK_EQ(MojoMapBuffer(shared_buffer, 0, 100, nullptr, &buffer),
             MOJO_RESULT_OK);
    ASSERT_EQ(0, memcmp(buffer, kHello, sizeof(kHello)));

    // Now we'll write some stuff to the shared buffer.
    static const char kWorld[] = "world!!!";
    memcpy(buffer, kWorld, sizeof(kWorld));

    // And send a message to signal that we've written stuff.
    const std::string go3("go 3");
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(h, &go3[0], static_cast<uint32_t>(go3.size()),
                               nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for |h| to become readable, which should fail.
    hss = HandleSignalsState();
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    ASSERT_FALSE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
    ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(shared_buffer));
  });
}

DEFINE_TEST_CLIENT_WITH_PIPE(CheckPlatformHandleFile,
                             MultiprocessMessagePipeTest,
                             h) {
  HandleSignalsState hss;
  CHECK_EQ(WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[255];  // Maximum number to receive.
  uint32_t num_handlers = std::size(handles);

  CHECK_EQ(MojoReadMessage(h, &read_buffer[0], &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  read_buffer.resize(num_bytes);
  char hello[32];
  int num_handles = 0;
  sscanf(read_buffer.c_str(), "%s %d", hello, &num_handles);
  CHECK_EQ(std::string("hello"), std::string(hello));
  CHECK_GT(num_handles, 0);

  for (int i = 0; i < num_handles; ++i) {
    PlatformHandle handle =
        UnwrapPlatformHandle(ScopedHandle(Handle(handles[i])));
    CHECK(handle.is_valid());

    base::ScopedFILE fp = test::FILEFromPlatformHandle(std::move(handle), "r");
    CHECK(fp);
    std::string fread_buffer(100, '\0');
    size_t bytes_read =
        fread(&fread_buffer[0], 1, fread_buffer.size(), fp.get());
    fread_buffer.resize(bytes_read);
    CHECK_EQ(fread_buffer, "world");
  }

  return 0;
}

#if !BUILDFLAG(IS_ANDROID)
class MultiprocessMessagePipeTestWithPipeCount
    : public MultiprocessMessagePipeTest,
      public testing::WithParamInterface<size_t> {};

TEST_P(MultiprocessMessagePipeTestWithPipeCount, PlatformHandlePassing) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  RunTestClient("CheckPlatformHandleFile", [&](MojoHandle h) {
    std::vector<MojoHandle> handles;

    size_t pipe_count = GetParam();
    for (size_t i = 0; i < pipe_count; ++i) {
      base::FilePath unused;
      base::ScopedFILE fp =
          CreateAndOpenTemporaryStreamInDir(temp_dir.GetPath(), &unused);
      const std::string world("world");
      CHECK_EQ(fwrite(&world[0], 1, world.size(), fp.get()), world.size());
      fflush(fp.get());
      rewind(fp.get());
      ScopedHandle handle =
          WrapPlatformHandle(test::PlatformHandleFromFILE(std::move(fp)));
      ASSERT_TRUE(handle.is_valid());
      handles.push_back(handle.release().value());
    }

    char message[128];
    snprintf(message, sizeof(message), "hello %d",
             static_cast<int>(pipe_count));
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(h, message, static_cast<uint32_t>(strlen(message)),
                         &handles[0], static_cast<uint32_t>(handles.size()),
                         MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for it to become readable, which should fail.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    ASSERT_FALSE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
    ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  });
}

// Android multi-process tests are not executing the new process. This is flaky.
INSTANTIATE_TEST_SUITE_P(PipeCount,
                         MultiprocessMessagePipeTestWithPipeCount,
                         // TODO(rockot): Enable the 128 and 250 pipe cases when
                         // ChannelPosix and ChannelFuchsia have support for
                         // sending larger numbers of handles per-message. See
                         // kMaxAttachedHandles in channel.cc for details.
                         testing::Values(1u, 64u /*, 128u, 250u*/));
#endif

DEFINE_TEST_CLIENT_WITH_PIPE(CheckMessagePipe, MultiprocessMessagePipeTest, h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = std::size(handles);
  CHECK_EQ(MojoReadMessage(h, nullptr, nullptr, &handles[0], &num_handlers,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(WaitForSignals(handles[0], MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0], &read_buffer_size,
                           nullptr, 0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()), nullptr,
                            0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, MessagePipePassing) {
  RunTestClient("CheckMessagePipe", [&](MojoHandle h) {
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_FLAG_NONE;

    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &mp1, &mp2));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(mp1, &hello[0], static_cast<uint32_t>(hello.size()),
                         nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h, nullptr, 0, &mp2, 1,
                                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(mp1, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0], &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));

    MojoClose(mp1);
  });
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, MessagePipeTwoPassing) {
  RunTestClient("CheckMessagePipe", [&](MojoHandle h) {
    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &mp2, &mp1));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(mp1, &hello[0], static_cast<uint32_t>(hello.size()),
                         nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h, nullptr, 0u, &mp2, 1u,
                                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(mp1, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0], &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));
    MojoClose(mp1);
  });
}

DEFINE_TEST_CLIENT_WITH_PIPE(DataPipeConsumer, MultiprocessMessagePipeTest, h) {
  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
               MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
               MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = std::size(handles);
  CHECK_EQ(MojoReadMessage(h, nullptr, nullptr, &handles[0], &num_handlers,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(WaitForSignals(handles[0], MOJO_HANDLE_SIGNAL_READABLE, &hss),
           MOJO_RESULT_OK);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  CHECK(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
               MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
               MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0], &read_buffer_size,
                           nullptr, 0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()), nullptr,
                            0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

TEST_F(MultiprocessMessagePipeTest, DataPipeConsumer) {
  RunTestClient("DataPipeConsumer", [&](MojoHandle h) {
    MojoCreateSharedBufferOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_SHARED_BUFFER_FLAG_NONE;

    MojoHandle mp1, mp2;
    ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &mp2, &mp1));

    // Write a string into one end of the new message pipe and send the other
    // end.
    const std::string hello("hello");
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(mp1, &hello[0], static_cast<uint32_t>(hello.size()),
                         nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));
    ASSERT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h, nullptr, 0, &mp2, 1u,
                                               MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Wait for a message from the child.
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(mp1, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(100, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0], &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    CHECK_EQ(read_buffer, std::string("world"));

    MojoClose(mp1);
  });
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, CreateMessagePipe) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  VerifyTransmission(p0, p1, std::string(10 * 1024 * 1024, 'a'));
  VerifyTransmission(p1, p0, std::string(10 * 1024 * 1024, 'e'));

  CloseHandle(p0);
  CloseHandle(p1);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, PassMessagePipeLocal) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  VerifyTransmission(p0, p1, "testing testing");
  VerifyTransmission(p1, p0, "one two three");

  MojoHandle p2, p3;

  CreateMessagePipe(&p2, &p3);
  VerifyTransmission(p2, p3, "testing testing");
  VerifyTransmission(p3, p2, "one two three");

  // Pass p2 over p0 to p1.
  const std::string message = "ceci n'est pas une pipe";
  WriteMessageWithHandles(p0, message, &p2, 1);
  EXPECT_EQ(message, ReadMessageWithHandles(p1, &p2, 1));

  CloseHandle(p0);
  CloseHandle(p1);

  // Verify that the received handle (now in p2) still works.
  VerifyTransmission(p2, p3, "Easy come, easy go; will you let me go?");
  VerifyTransmission(p3, p2, "Bismillah! NO! We will not let you go!");

  CloseHandle(p2);
  CloseHandle(p3);
}

// Echos the primordial channel until "exit".
DEFINE_TEST_CLIENT_WITH_PIPE(ChannelEchoClient,
                             MultiprocessMessagePipeTest,
                             h) {
  for (;;) {
    std::string message = ReadMessage(h);
    if (message == "exit")
      break;
    WriteMessage(h, message);
  }
  return 0;
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, MultiprocessChannelPipe) {
  RunTestClient("ChannelEchoClient", [&](MojoHandle h) {
    VerifyEcho(h, "in an interstellar burst");
    VerifyEcho(h, "i am back to save the universe");
    VerifyEcho(h, std::string(10 * 1024 * 1024, 'o'));

    WriteMessage(h, "exit");
  });
}

// Receives a pipe handle from the primordial channel and echos on it until
// "exit". Used to test simple pipe transfer across processes via channels.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoServiceClient,
                             MultiprocessMessagePipeTest,
                             h) {
  MojoHandle p;
  ReadMessageWithHandles(h, &p, 1);
  for (;;) {
    std::string message = ReadMessage(p);
    if (message == "exit")
      break;
    WriteMessage(p, message);
  }
  CloseHandle(p);
  return 0;
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport,
       PassMessagePipeCrossProcess) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);
  RunTestClient("EchoServiceClient", [&](MojoHandle h) {
    // Pass one end of the pipe to the other process.
    WriteMessageWithHandles(h, "here take this", &p1, 1);

    VerifyEcho(p0, "and you may ask yourself");
    VerifyEcho(p0, "where does that highway go?");
    VerifyEcho(p0, std::string(20 * 1024 * 1024, 'i'));

    WriteMessage(p0, "exit");
  });
  CloseHandle(p0);
}

// Receives a pipe handle from the primordial channel and reads new handles
// from it. Each read handle establishes a new echo channel.
DEFINE_TEST_CLIENT_WITH_PIPE(EchoServiceFactoryClient,
                             MultiprocessMessagePipeTest,
                             h) {
  MojoHandle p;
  ReadMessageWithHandles(h, &p, 1);

  std::vector<Handle> handles(2);
  handles[0] = Handle(h);
  handles[1] = Handle(p);
  std::vector<MojoHandleSignals> signals(2, MOJO_HANDLE_SIGNAL_READABLE);
  for (;;) {
    size_t index;
    CHECK_EQ(
        mojo::WaitMany(handles.data(), signals.data(), handles.size(), &index),
        MOJO_RESULT_OK);
    DCHECK_LE(index, handles.size());
    if (index == 0) {
      // If data is available on the first pipe, it should be an exit command.
      EXPECT_EQ(std::string("exit"), ReadMessage(h));
      break;
    } else if (index == 1) {
      // If the second pipe, it should be a new handle requesting echo service.
      MojoHandle echo_request;
      ReadMessageWithHandles(p, &echo_request, 1);
      handles.push_back(Handle(echo_request));
      signals.push_back(MOJO_HANDLE_SIGNAL_READABLE);
    } else {
      // Otherwise it was one of our established echo pipes. Echo!
      WriteMessage(handles[index].value(), ReadMessage(handles[index].value()));
    }
  }

  for (size_t i = 1; i < handles.size(); ++i)
    CloseHandle(handles[i].value());

  return 0;
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport,
       PassMoarMessagePipesCrossProcess) {
  MojoHandle echo_factory_proxy, echo_factory_request;
  CreateMessagePipe(&echo_factory_proxy, &echo_factory_request);

  MojoHandle echo_proxy_a, echo_request_a;
  CreateMessagePipe(&echo_proxy_a, &echo_request_a);

  MojoHandle echo_proxy_b, echo_request_b;
  CreateMessagePipe(&echo_proxy_b, &echo_request_b);

  MojoHandle echo_proxy_c, echo_request_c;
  CreateMessagePipe(&echo_proxy_c, &echo_request_c);

  RunTestClient("EchoServiceFactoryClient", [&](MojoHandle h) {
    WriteMessageWithHandles(h, "gief factory naow plz", &echo_factory_request,
                            1);

    WriteMessageWithHandles(echo_factory_proxy, "give me an echo service plz!",
                            &echo_request_a, 1);
    WriteMessageWithHandles(echo_factory_proxy, "give me one too!",
                            &echo_request_b, 1);

    VerifyEcho(echo_proxy_a, "i came here for an argument");
    VerifyEcho(echo_proxy_a, "shut your festering gob");
    VerifyEcho(echo_proxy_a, "mumble mumble mumble");

    VerifyEcho(echo_proxy_b, "wubalubadubdub");
    VerifyEcho(echo_proxy_b, "wubalubadubdub");

    WriteMessageWithHandles(echo_factory_proxy, "hook me up also thanks",
                            &echo_request_c, 1);

    VerifyEcho(echo_proxy_a, "the frobinators taste like frobinators");
    VerifyEcho(echo_proxy_b, "beep bop boop");
    VerifyEcho(echo_proxy_c, "zzzzzzzzzzzzzzzzzzzzzzzzzz");

    WriteMessage(h, "exit");
  });

  CloseHandle(echo_factory_proxy);
  CloseHandle(echo_proxy_a);
  CloseHandle(echo_proxy_b);
  CloseHandle(echo_proxy_c);
}

// Flaky on Android. See https://crbug.com/905620.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ChannelPipesWithMultipleChildren \
  DISABLED_ChannelPipesWithMultipleChildren
#else
#define MAYBE_ChannelPipesWithMultipleChildren ChannelPipesWithMultipleChildren
#endif
TEST_P(MultiprocessMessagePipeTestWithPeerSupport,
       MAYBE_ChannelPipesWithMultipleChildren) {
  RunTestClient("ChannelEchoClient", [&](MojoHandle a) {
    RunTestClient("ChannelEchoClient", [&](MojoHandle b) {
      VerifyEcho(a, "hello child 0");
      VerifyEcho(b, "hello child 1");

      WriteMessage(a, "exit");
      WriteMessage(b, "exit");
    });
  });
}

// Reads and turns a pipe handle some number of times to create lots of
// transient proxies.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(PingPongPipeClient,
                                  MultiprocessMessagePipeTest,
                                  h) {
  const size_t kNumBounces = 50;
  MojoHandle p0, p1;
  ReadMessageWithHandles(h, &p0, 1);
  ReadMessageWithHandles(h, &p1, 1);
  for (size_t i = 0; i < kNumBounces; ++i) {
    WriteMessageWithHandles(h, "", &p1, 1);
    ReadMessageWithHandles(h, &p1, 1);
  }
  WriteMessageWithHandles(h, "", &p0, 1);
  WriteMessage(p1, "bye");
  MojoClose(p1);
  EXPECT_EQ("quit", ReadMessage(h));
  MojoClose(h);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, PingPongPipe) {
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  RunTestClient("PingPongPipeClient", [&](MojoHandle h) {
    const size_t kNumBounces = 50;
    WriteMessageWithHandles(h, "", &p0, 1);
    WriteMessageWithHandles(h, "", &p1, 1);
    for (size_t i = 0; i < kNumBounces; ++i) {
      ReadMessageWithHandles(h, &p1, 1);
      WriteMessageWithHandles(h, "", &p1, 1);
    }
    ReadMessageWithHandles(h, &p0, 1);
    EXPECT_EQ("bye", ReadMessage(p0));
    WriteMessage(h, "quit");
  });

  // We should still be able to observe peer closure from the other end.
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(p0, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  MojoClose(p0);
}

// Parses commands from the parent pipe and does whatever it's asked to do.
DEFINE_TEST_CLIENT_WITH_PIPE(CommandDrivenClient,
                             MultiprocessMessagePipeTest,
                             h) {
  std::unordered_map<std::string, MojoHandle> named_pipes;
  for (;;) {
    MojoHandle p;
    auto parts = base::SplitString(ReadMessageWithOptionalHandle(h, &p), ":",
                                   base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK(!parts.empty());
    std::string command = parts[0];
    if (command == "take") {
      // Take a pipe.
      CHECK_EQ(parts.size(), 2u);
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes[parts[1]] = p;
      WriteMessage(h, "ok");
    } else if (command == "return") {
      // Return a pipe.
      CHECK_EQ(parts.size(), 2u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes.erase(parts[1]);
      WriteMessageWithHandles(h, "ok", &p, 1);
    } else if (command == "say") {
      // Say something to a named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK(!parts[2].empty());
      WriteMessage(p, parts[2]);
      WriteMessage(h, "ok");
    } else if (command == "hear") {
      // Expect to read something from a named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK(!parts[2].empty());
      CHECK_EQ(parts[2], ReadMessage(p));
      WriteMessage(h, "ok");
    } else if (command == "pass") {
      // Pass one named pipe over another named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      p = named_pipes[parts[1]];
      MojoHandle carrier = named_pipes[parts[2]];
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      CHECK_NE(carrier, MOJO_HANDLE_INVALID);
      named_pipes.erase(parts[1]);
      WriteMessageWithHandles(carrier, "got a pipe for ya", &p, 1);
      WriteMessage(h, "ok");
    } else if (command == "catch") {
      // Expect to receive one named pipe from another named pipe.
      CHECK_EQ(parts.size(), 3u);
      CHECK_EQ(p, MOJO_HANDLE_INVALID);
      MojoHandle carrier = named_pipes[parts[2]];
      CHECK_NE(carrier, MOJO_HANDLE_INVALID);
      ReadMessageWithHandles(carrier, &p, 1);
      CHECK_NE(p, MOJO_HANDLE_INVALID);
      named_pipes[parts[1]] = p;
      WriteMessage(h, "ok");
    } else if (command == "exit") {
      CHECK_EQ(parts.size(), 1u);
      break;
    }
  }

  for (auto& pipe : named_pipes)
    CloseHandle(pipe.second);

  return 0;
}

TEST_F(MultiprocessMessagePipeTest, ChildToChildPipes) {
  RunTestClient("CommandDrivenClient", [&](MojoHandle h0) {
    RunTestClient("CommandDrivenClient", [&](MojoHandle h1) {
      CommandDrivenClientController a(h0);
      CommandDrivenClientController b(h1);

      // Create a pipe and pass each end to a different client.
      MojoHandle p0, p1;
      CreateMessagePipe(&p0, &p1);
      a.SendHandle("x", p0);
      b.SendHandle("y", p1);

      // Make sure they can talk.
      a.Send("say:x:hello");
      b.Send("hear:y:hello");

      b.Send("say:y:i love multiprocess pipes!");
      a.Send("hear:x:i love multiprocess pipes!");

      a.Exit();
      b.Exit();
    });
  });
}

TEST_F(MultiprocessMessagePipeTest, MoreChildToChildPipes) {
  RunTestClient("CommandDrivenClient", [&](MojoHandle h0) {
    RunTestClient("CommandDrivenClient", [&](MojoHandle h1) {
      RunTestClient("CommandDrivenClient", [&](MojoHandle h2) {
        RunTestClient("CommandDrivenClient", [&](MojoHandle h3) {
          CommandDrivenClientController a(h0), b(h1), c(h2), d(h3);

          // Connect a to b and c to d

          MojoHandle p0, p1;

          CreateMessagePipe(&p0, &p1);
          a.SendHandle("b_pipe", p0);
          b.SendHandle("a_pipe", p1);

          MojoHandle p2, p3;

          CreateMessagePipe(&p2, &p3);
          c.SendHandle("d_pipe", p2);
          d.SendHandle("c_pipe", p3);

          // Connect b to c via a and d
          MojoHandle p4, p5;
          CreateMessagePipe(&p4, &p5);
          a.SendHandle("d_pipe", p4);
          d.SendHandle("a_pipe", p5);

          // Have |a| pass its new |d|-pipe to |b|. It will eventually connect
          // to |c|.
          a.Send("pass:d_pipe:b_pipe");
          b.Send("catch:c_pipe:a_pipe");

          // Have |d| pass its new |a|-pipe to |c|. It will now be connected to
          // |b|.
          d.Send("pass:a_pipe:c_pipe");
          c.Send("catch:b_pipe:d_pipe");

          // Make sure b and c and talk.
          b.Send("say:c_pipe:it's a beautiful day");
          c.Send("hear:b_pipe:it's a beautiful day");

          // Create x and y and have b and c exchange them.
          MojoHandle x, y;
          CreateMessagePipe(&x, &y);
          b.SendHandle("x", x);
          c.SendHandle("y", y);
          b.Send("pass:x:c_pipe");
          c.Send("pass:y:b_pipe");
          b.Send("catch:y:c_pipe");
          c.Send("catch:x:b_pipe");

          // Make sure the pipe still works in both directions.
          b.Send("say:y:hello");
          c.Send("hear:x:hello");
          c.Send("say:x:goodbye");
          b.Send("hear:y:goodbye");

          // Take both pipes back.
          y = c.RetrieveHandle("x");
          x = b.RetrieveHandle("y");

          VerifyTransmission(x, y, "still works");
          VerifyTransmission(y, x, "in both directions");

          CloseHandle(x);
          CloseHandle(y);

          a.Exit();
          b.Exit();
          c.Exit();
          d.Exit();
        });
      });
    });
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceivePipeWithClosedPeer,
                                  MultiprocessMessagePipeTest,
                                  h) {
  MojoHandle p;
  EXPECT_EQ("foo", ReadMessageWithHandles(h, &p, 1));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(p, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  MojoClose(p);
  MojoClose(h);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, SendPipeThenClosePeer) {
  RunTestClient("ReceivePipeWithClosedPeer", [&](MojoHandle h) {
    MojoHandle a, b;
    CreateMessagePipe(&a, &b);

    // Send |a| and immediately close |b|. The child should observe closure.
    WriteMessageWithHandles(h, "foo", &a, 1);
    MojoClose(b);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(SendOtherChildPipeWithClosedPeer,
                                  MultiprocessMessagePipeTest,
                                  h) {
  // Create a new pipe and send one end to the parent, who will connect it to
  // a client running ReceivePipeWithClosedPeerFromOtherChild.
  MojoHandle application_proxy, application_request;
  CreateMessagePipe(&application_proxy, &application_request);
  WriteMessageWithHandles(h, "c2a plz", &application_request, 1);

  // Create another pipe and send one end to the remote "application".
  MojoHandle service_proxy, service_request;
  CreateMessagePipe(&service_proxy, &service_request);
  WriteMessageWithHandles(application_proxy, "c2s lol", &service_request, 1);

  // Immediately close the service proxy. The "application" should detect this.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(service_proxy));

  // Wait for quit.
  EXPECT_EQ("quit", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(application_proxy));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceivePipeWithClosedPeerFromOtherChild,
                                  MultiprocessMessagePipeTest,
                                  h) {
  // Receive a pipe from the parent. This is akin to an "application request".
  MojoHandle application_client;
  EXPECT_EQ("c2a", ReadMessageWithHandles(h, &application_client, 1));

  // Receive a pipe from the "application" "client".
  MojoHandle service_client;
  EXPECT_EQ("c2s lol",
            ReadMessageWithHandles(application_client, &service_client, 1));

  // Wait for the service client to signal closure.
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(service_client, MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(service_client));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(application_client));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

#if BUILDFLAG(IS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_SendPipeWithClosedPeerBetweenChildren \
  DISABLED_SendPipeWithClosedPeerBetweenChildren
#else
#define MAYBE_SendPipeWithClosedPeerBetweenChildren \
  SendPipeWithClosedPeerBetweenChildren
#endif
TEST_F(MultiprocessMessagePipeTest,
       MAYBE_SendPipeWithClosedPeerBetweenChildren) {
  RunTestClient("SendOtherChildPipeWithClosedPeer", [&](MojoHandle kid_a) {
    RunTestClient(
        "ReceivePipeWithClosedPeerFromOtherChild", [&](MojoHandle kid_b) {
          // Receive an "application request" from the first child and forward
          // it to the second child.
          MojoHandle application_request;
          EXPECT_EQ("c2a plz",
                    ReadMessageWithHandles(kid_a, &application_request, 1));

          WriteMessageWithHandles(kid_b, "c2a", &application_request, 1);
        });

    WriteMessage(kid_a, "quit");
  });
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, SendClosePeerSend) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Send |a| over |c|, immediately close |b|, then send |a| back over |d|.
  WriteMessageWithHandles(c, "foo", &a, 1);
  EXPECT_EQ("foo", ReadMessageWithHandles(d, &a, 1));
  WriteMessageWithHandles(d, "bar", &a, 1);
  EXPECT_EQ("bar", ReadMessageWithHandles(c, &a, 1));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  // We should be able to detect peer closure on |a|.
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(a, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  MojoClose(a);
  MojoClose(c);
  MojoClose(d);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(WriteCloseSendPeerClient,
                                  MultiprocessMessagePipeTest,
                                  h) {
  MojoHandle pipe[2];
  EXPECT_EQ("foo", ReadMessageWithHandles(h, pipe, 2));

  // Write some messages to the first endpoint and then close it.
  WriteMessage(pipe[0], "baz");
  WriteMessage(pipe[0], "qux");
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe[0]));

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Pass the orphaned endpoint over another pipe before passing it back to
  // the parent, just for some extra proxying goodness.
  WriteMessageWithHandles(c, "foo", &pipe[1], 1);
  EXPECT_EQ("foo", ReadMessageWithHandles(d, &pipe[1], 1));

  // And finally pass it back to the parent.
  WriteMessageWithHandles(h, "bar", &pipe[1], 1);

  EXPECT_EQ("quit", ReadMessage(h));
  MojoClose(h);
  MojoClose(c);
  MojoClose(d);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport, WriteCloseSendPeer) {
  MojoHandle pipe[2];
  CreateMessagePipe(&pipe[0], &pipe[1]);

  RunTestClient("WriteCloseSendPeerClient", [&](MojoHandle h) {
    // Pass the pipe to the child.
    WriteMessageWithHandles(h, "foo", pipe, 2);

    // Read back an endpoint which should have messages on it.
    MojoHandle p;
    EXPECT_EQ("bar", ReadMessageWithHandles(h, &p, 1));

    EXPECT_EQ("baz", ReadMessage(p));
    EXPECT_EQ("qux", ReadMessage(p));

    // Expect to have peer closure signaled.
    EXPECT_EQ(MOJO_RESULT_OK,
              WaitForSignals(p, MOJO_HANDLE_SIGNAL_PEER_CLOSED));

    WriteMessage(h, "quit");
    MojoClose(p);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(MessagePipeStatusChangeInTransitClient,
                                  MultiprocessMessagePipeTest,
                                  parent) {
  // This test verifies that peer closure is detectable through various
  // mechanisms when it races with handle transfer.
  MojoHandle handles[4];
  EXPECT_EQ("o_O", ReadMessageWithHandles(parent, handles, 4));

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(handles[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  base::test::SingleThreadTaskEnvironment task_environment;

  // Wait on handle 1 using a SimpleWatcher.
  {
    base::RunLoop run_loop;
    SimpleWatcher watcher(FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC,
                          base::SequencedTaskRunner::GetCurrentDefault());
    watcher.Watch(Handle(handles[1]), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                  base::BindRepeating(
                      [](base::RunLoop* loop, MojoResult result) {
                        EXPECT_EQ(MOJO_RESULT_OK, result);
                        loop->Quit();
                      },
                      &run_loop));
    run_loop.Run();
  }

  // Wait on handle 2 by polling with MojoReadMessage.
  MojoResult result;
  do {
    result = MojoReadMessage(handles[2], nullptr, nullptr, nullptr, nullptr,
                             MOJO_READ_MESSAGE_FLAG_NONE);
  } while (result == MOJO_RESULT_SHOULD_WAIT);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  // Wait on handle 3 by polling with MojoWriteMessage.
  do {
    result = MojoWriteMessage(handles[3], nullptr, 0, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE);
  } while (result == MOJO_RESULT_OK);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  for (size_t i = 0; i < 4; ++i)
    CloseHandle(handles[i]);
  CloseHandle(parent);
}

TEST_P(MultiprocessMessagePipeTestWithPeerSupport,
       ReceiveMessagesSentJustBeforeProcessDeath) {
  // Regression test for https://crbug.com/1005510. The client will write a
  // message to the pipe it gives us and then it will die immediately. We should
  // always be able to read the message received on that pipe.
  RunTestClient("SpotaneouslyDyingProcess", [&](MojoHandle child) {
    MojoHandle receiver;
    VerifyEcho(child, "!");
    EXPECT_EQ("receiver", ReadMessageWithHandles(child, &receiver, 1));
    EXPECT_EQ("ok", ReadMessage(receiver));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(receiver));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(SpotaneouslyDyingProcess,
                                  MultiprocessMessagePipeTest,
                                  parent) {
  MojoHandle sender;
  MojoHandle receiver;
  CreateMessagePipe(&sender, &receiver);

  VerifyEcho(parent, "!");
  WriteMessageWithHandles(parent, "receiver", &receiver, 1);

  if (!IsMojoIpczEnabled()) {
    // Wait for the pipe to actually appear as remote. Before this happens, it's
    // possible for message transmission to be deferred to the IO thread, and
    // sudden termination might preempt that work. Note that this is unnecessary
    // (and PEER_REMOTE signals are unsupported anyway) with MojoIpcz.
    WaitForSignals(sender, MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  }

  WriteMessage(sender, "ok");
  MojoClose(sender);
  MojoClose(parent);

  // Here process termination is imminent. If the bug reappears this test will
  // fail flakily.
}

TEST_F(MultiprocessMessagePipeTest, MessagePipeStatusChangeInTransit) {
  MojoHandle local_handles[4];
  MojoHandle sent_handles[4];
  for (size_t i = 0; i < 4; ++i)
    CreateMessagePipe(&local_handles[i], &sent_handles[i]);

  RunTestClient("MessagePipeStatusChangeInTransitClient",
                [&](MojoHandle child) {
                  // Send 4 handles and let their transfer race with their
                  // peers' closure.
                  WriteMessageWithHandles(child, "o_O", sent_handles, 4);
                  for (size_t i = 0; i < 4; ++i)
                    CloseHandle(local_handles[i]);
                });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(BadMessageClient,
                                  MultiprocessMessagePipeTest,
                                  parent) {
  MojoHandle pipe;
  EXPECT_EQ("hi", ReadMessageWithHandles(parent, &pipe, 1));
  WriteMessage(pipe, "derp");
  EXPECT_EQ("bye", ReadMessage(parent));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiprocessMessagePipeTestWithPeerSupport,
    testing::Values(test::MojoTestBase::LaunchType::CHILD,
                    test::MojoTestBase::LaunchType::CHILD_WITHOUT_CAPABILITIES,
                    test::MojoTestBase::LaunchType::PEER,
                    test::MojoTestBase::LaunchType::ASYNC
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
                    // Fuchsia has no named pipe support.
                    ,
                    test::MojoTestBase::LaunchType::NAMED_CHILD,
                    test::MojoTestBase::LaunchType::NAMED_PEER
#endif  // !BUILDFLAG(IS_FUCHSIA)
                    ));
}  // namespace
}  // namespace core
}  // namespace mojo
