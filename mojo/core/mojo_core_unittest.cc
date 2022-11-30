// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "mojo/core/mojo_core_unittest.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace switches {
const char kMojoLoadBeforeInit[] = "mojo-load-before-init";
const char kMojoUseExplicitLibraryPath[] = "mojo-use-explicit-library-path";
}  // namespace switches

namespace {

// TODO(https://crbug.com/902135): Re-enable this on MSAN. Currently hangs
// because of an apparent deadlock in MSAN's fork() in multithreaded
// environments.
#if !defined(MEMORY_SANITIZER)

uint64_t kTestPipeName = 0;
const char kTestMessage[] = "hai";
const char kTestReply[] = "bai";

std::string ReadMessageAsString(mojo::MessagePipeHandle handle) {
  std::vector<uint8_t> data;
  CHECK_EQ(MOJO_RESULT_OK, mojo::ReadMessageRaw(handle, &data, nullptr,
                                                MOJO_READ_MESSAGE_FLAG_NONE));
  return std::string(data.begin(), data.end());
}

TEST(MojoCoreTest, SanityCheck) {
  // Exercises some APIs against the mojo_core library and expects them to work
  // as intended.

  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &a, &b));

  MojoMessageHandle m;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &m));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetMessageContext(m, 42, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoWriteMessage(a, m, nullptr));
  m = MOJO_MESSAGE_HANDLE_INVALID;

  MojoHandleSignalsState state;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, nullptr, &m));

  uintptr_t context = 0;
  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageContext(m, nullptr, &context));
  EXPECT_EQ(42u, context);

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(m));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
}

enum class InitializationMode { kCombinedLoadAndInit, kLoadBeforeInit };

enum class LoadPathSpec { kImplicit, kExplicit };

class MojoCoreMultiprocessTest
    : public ::testing::TestWithParam<
          std::tuple<InitializationMode, LoadPathSpec>> {
 public:
  void SetChildCommandLineForTestParams(base::CommandLine* command_line) {
    if (std::get<0>(GetParam()) == InitializationMode::kLoadBeforeInit)
      command_line->AppendSwitch(switches::kMojoLoadBeforeInit);
    if (std::get<1>(GetParam()) == LoadPathSpec::kExplicit)
      command_line->AppendSwitch(switches::kMojoUseExplicitLibraryPath);
  }
};

TEST_P(MojoCoreMultiprocessTest, BasicMultiprocess) {
  base::CommandLine child_cmd(base::GetMultiProcessTestChildBaseCommandLine());
  SetChildCommandLineForTestParams(&child_cmd);

  base::LaunchOptions options;
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&options, &child_cmd);
  base::Process child_process = base::SpawnMultiProcessTestChild(
      "BasicMultiprocessClientMain", child_cmd, options);
  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation invitation;
  auto child_pipe = invitation.AttachMessagePipe(kTestPipeName);
  mojo::OutgoingInvitation::Send(std::move(invitation), child_process.Handle(),
                                 channel.TakeLocalEndpoint());

  mojo::Wait(child_pipe.get(), MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kTestMessage, ReadMessageAsString(child_pipe.get()));

  mojo::WriteMessageRaw(child_pipe.get(), kTestReply, sizeof(kTestReply) - 1,
                        nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);

  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(BasicMultiprocessClientMain) {
  auto endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto parent_pipe = invitation.ExtractMessagePipe(kTestPipeName);

  mojo::WriteMessageRaw(parent_pipe.get(), kTestMessage,
                        sizeof(kTestMessage) - 1, nullptr, 0,
                        MOJO_WRITE_MESSAGE_FLAG_NONE);

  mojo::Wait(parent_pipe.get(), MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kTestReply, ReadMessageAsString(parent_pipe.get()));

  return 0;
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MojoCoreMultiprocessTest,
    ::testing::Combine(
        ::testing::Values(InitializationMode::kCombinedLoadAndInit,
                          InitializationMode::kLoadBeforeInit),
        ::testing::Values(LoadPathSpec::kImplicit, LoadPathSpec::kExplicit)));

#endif  // !defined(MEMORY_SANITIZER)

}  // namespace
