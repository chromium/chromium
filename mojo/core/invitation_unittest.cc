// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/c/system/invitation.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/test_switches.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#include "mojo/core/core.h"
#include "mojo/core/node_controller.h"
#endif

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include "base/apple/mach_port_rendezvous.h"
#endif

namespace mojo {
namespace core {
namespace {

const char kSecondaryChannelHandleSwitch[] = "test-secondary-channel-handle";

// TODO(crbug.com/40900578): Flaky on Tsan.
#if defined(THREAD_SANITIZER)
#define MAYBE_InvitationTest DISABLED_InvitationTest
#else
#define MAYBE_InvitationTest InvitationTest
#endif
class MAYBE_InvitationTest : public test::MojoTestBase {
 public:
  MAYBE_InvitationTest() = default;

  MAYBE_InvitationTest(const MAYBE_InvitationTest&) = delete;
  MAYBE_InvitationTest& operator=(const MAYBE_InvitationTest&) = delete;

  ~MAYBE_InvitationTest() override = default;

 protected:
  static base::Process LaunchChildTestClient(
      const std::string& test_client_name,
      MojoHandle* primordial_pipes,
      size_t num_primordial_pipes,
      MojoSendInvitationFlags send_flags,
      MojoProcessErrorHandler error_handler = nullptr,
      uintptr_t error_handler_context = 0,
      base::CommandLine* custom_command_line = nullptr,
      base::LaunchOptions* custom_launch_options = nullptr);

  static void SendInvitationToClient(PlatformHandle endpoint_handle,
                                     base::ProcessHandle process,
                                     MojoHandle* primordial_pipes,
                                     size_t num_primordial_pipes,
                                     MojoSendInvitationFlags flags,
                                     MojoProcessErrorHandler error_handler,
                                     uintptr_t error_handler_context,
                                     std::string_view isolated_invitation_name);

  static void WaitForProcessToTerminate(base::Process& process) {
    int wait_result = -1;
    base::WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_timeout(), &wait_result);
    EXPECT_EQ(0, wait_result);
    process.Close();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

void PrepareToPassRemoteEndpoint(PlatformChannel* channel,
                                 base::LaunchOptions* options,
                                 base::CommandLine* command_line,
                                 std::string_view switch_name = {}) {
  const std::string value = channel->PrepareToPassRemoteEndpoint(*options);
  if (switch_name.empty()) {
    switch_name = PlatformChannel::kHandleSwitch;
  }
  command_line->AppendSwitchASCII(std::string(switch_name), value);
}

TEST_F(MAYBE_InvitationTest, Create) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  MojoCreateInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_INVITATION_FLAG_NONE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(&options, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
}

TEST_F(MAYBE_InvitationTest, InvalidArguments) {
  MojoHandle invitation;
  MojoCreateInvitationOptions invalid_create_options;
  invalid_create_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateInvitation(&invalid_create_options, &invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateInvitation(nullptr, nullptr));

  // We need a valid invitation handle to exercise some of the other invalid
  // argument cases below.
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe;
  MojoAttachMessagePipeToInvitationOptions invalid_attach_options;
  invalid_attach_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAttachMessagePipeToInvitation(MOJO_HANDLE_INVALID, "x", 1,
                                              nullptr, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAttachMessagePipeToInvitation(invitation, "x", 1,
                                              &invalid_attach_options, &pipe));
  EXPECT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      MojoAttachMessagePipeToInvitation(invitation, "x", 1, nullptr, nullptr));

  MojoExtractMessagePipeFromInvitationOptions invalid_extract_options;
  invalid_extract_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoExtractMessagePipeFromInvitation(MOJO_HANDLE_INVALID, "x", 1,
                                                 nullptr, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoExtractMessagePipeFromInvitation(
                invitation, "x", 1, &invalid_extract_options, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoExtractMessagePipeFromInvitation(invitation, "x", 1, nullptr,
                                                 nullptr));

  PlatformChannel channel;
  MojoPlatformHandle endpoint_handle;
  endpoint_handle.struct_size = sizeof(endpoint_handle);
  PlatformHandle::ToMojoPlatformHandle(
      channel.TakeLocalEndpoint().TakePlatformHandle(), &endpoint_handle);
  ASSERT_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint valid_endpoint;
  valid_endpoint.struct_size = sizeof(valid_endpoint);
  valid_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  valid_endpoint.num_platform_handles = 1;
  valid_endpoint.platform_handles = &endpoint_handle;

  MojoSendInvitationOptions invalid_send_options;
  invalid_send_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(MOJO_HANDLE_INVALID, nullptr, &valid_endpoint,
                               nullptr, 0, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &valid_endpoint, nullptr, 0,
                               &invalid_send_options));

  MojoInvitationTransportEndpoint invalid_endpoint;
  invalid_endpoint.struct_size = 0;
  invalid_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = &endpoint_handle;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  invalid_endpoint.struct_size = sizeof(invalid_endpoint);
  invalid_endpoint.num_platform_handles = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  MojoPlatformHandle invalid_platform_handle;
  invalid_platform_handle.struct_size = 0;
  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = &invalid_platform_handle;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));
  invalid_platform_handle.struct_size = sizeof(invalid_platform_handle);
  invalid_platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  MojoHandle accepted_invitation;
  MojoAcceptInvitationOptions invalid_accept_options;
  invalid_accept_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(nullptr, nullptr, &accepted_invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(&valid_endpoint, &invalid_accept_options,
                                 &accepted_invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(&valid_endpoint, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
}

TEST_F(MAYBE_InvitationTest, AttachAndExtractLocally) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAttachMessagePipeToInvitation(
                                invitation, "x", 1, nullptr, &pipe0));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe0);

  MojoHandle pipe1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, "x", 1, nullptr, &pipe1));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe1);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  // Should be able to communicate over the pipe.
  const std::string kMessage = "RSVP LOL";
  WriteMessage(pipe0, kMessage);
  EXPECT_EQ(kMessage, ReadMessage(pipe1));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

TEST_F(MAYBE_InvitationTest, ClosedInvitationClosesAttachments) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAttachMessagePipeToInvitation(
                                invitation, "x", 1, nullptr, &pipe));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe);

  // Closing the invitation should close |pipe|'s peer.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe));
}

TEST_F(MAYBE_InvitationTest, AttachNameInUse) {
  constexpr uint32_t kName0 = 0;
  constexpr uint32_t kName1 = 1;
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, &kName0,
                                              sizeof(kName0), nullptr, &pipe0));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe0);

  MojoHandle pipe1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            MojoAttachMessagePipeToInvitation(invitation, &kName0,
                                              sizeof(kName0), nullptr, &pipe1));
  EXPECT_EQ(MOJO_HANDLE_INVALID, pipe1);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, &kName1,
                                              sizeof(kName1), nullptr, &pipe1));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe1);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

// static
base::Process MAYBE_InvitationTest::LaunchChildTestClient(
    const std::string& test_client_name,
    MojoHandle* primordial_pipes,
    size_t num_primordial_pipes,
    MojoSendInvitationFlags send_flags,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    base::CommandLine* custom_command_line,
    base::LaunchOptions* custom_launch_options) {
  base::CommandLine default_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::CommandLine& command_line =
      custom_command_line ? *custom_command_line : default_command_line;

  // If this is called from a test child process launching yet another child
  // process, ensure this switch isn't set so that `SpawnMultiprocessTestChild`
  // sets it properly.
  command_line.RemoveSwitch(switches::kTestChildProcess);

  base::LaunchOptions default_launch_options;
  base::LaunchOptions& launch_options =
      custom_launch_options ? *custom_launch_options : default_launch_options;
#if BUILDFLAG(IS_WIN)
  launch_options.start_hidden = true;
#endif

  PlatformChannel channel;
  PlatformHandle local_endpoint_handle;
  PrepareToPassRemoteEndpoint(&channel, &launch_options, &command_line);
  local_endpoint_handle = channel.TakeLocalEndpoint().TakePlatformHandle();

  std::string enable_features;
  std::string disable_features;
  base::FeatureList::GetInstance()->GetCommandLineFeatureOverrides(
      &enable_features, &disable_features);
  command_line.AppendSwitchASCII(switches::kEnableFeatures, enable_features);
  command_line.AppendSwitchASCII(switches::kDisableFeatures, disable_features);

  if (send_flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) {
    command_line.AppendSwitch(test_switches::kMojoIsBroker);
  }

  base::Process child_process = base::SpawnMultiProcessTestChild(
      test_client_name, command_line, launch_options);
  channel.RemoteProcessLaunchAttempted();

  SendInvitationToClient(std::move(local_endpoint_handle),
                         child_process.Handle(), primordial_pipes,
                         num_primordial_pipes, send_flags, error_handler,
                         error_handler_context, "");

  return child_process;
}

// static
void MAYBE_InvitationTest::SendInvitationToClient(
    PlatformHandle endpoint_handle,
    base::ProcessHandle process,
    MojoHandle* primordial_pipes,
    size_t num_primordial_pipes,
    MojoSendInvitationFlags flags,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    std::string_view isolated_invitation_name) {
  MojoPlatformHandle handle;
  PlatformHandle::ToMojoPlatformHandle(std::move(endpoint_handle), &handle);
  CHECK_NE(handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoHandle invitation;
  CHECK_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));
  for (uint32_t name = 0; name < num_primordial_pipes; ++name) {
    CHECK_EQ(MOJO_RESULT_OK,
             MojoAttachMessagePipeToInvitation(invitation, &name, 4, nullptr,
                                               &primordial_pipes[name]));
  }

  MojoPlatformProcessHandle process_handle;
  process_handle.struct_size = sizeof(process_handle);
#if BUILDFLAG(IS_WIN)
  process_handle.value =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(process));
#else
  process_handle.value = static_cast<uint64_t>(process);
#endif

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &handle;

  MojoSendInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  if (flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) {
    options.isolated_connection_name = isolated_invitation_name.data();
    options.isolated_connection_name_length =
        static_cast<uint32_t>(isolated_invitation_name.size());
  }
  CHECK_EQ(MOJO_RESULT_OK,
           MojoSendInvitation(invitation, &process_handle, &transport_endpoint,
                              error_handler, error_handler_context, &options));
}

class TestClientBase : public MAYBE_InvitationTest {
 public:
  TestClientBase(const TestClientBase&) = delete;
  TestClientBase& operator=(const TestClientBase&) = delete;

  static MojoHandle AcceptInvitation(MojoAcceptInvitationFlags flags,
                                     std::string_view switch_name = {}) {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
    PlatformChannelEndpoint channel_endpoint;
    if (switch_name.empty()) {
      channel_endpoint =
          PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
    } else {
      channel_endpoint = PlatformChannel::RecoverPassedEndpointFromString(
          command_line.GetSwitchValueASCII(switch_name));
    }
    MojoPlatformHandle endpoint_handle;
    PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                         &endpoint_handle);
    CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

    MojoInvitationTransportEndpoint transport_endpoint;
    transport_endpoint.struct_size = sizeof(transport_endpoint);
    transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
    transport_endpoint.num_platform_handles = 1;
    transport_endpoint.platform_handles = &endpoint_handle;

    MojoAcceptInvitationOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    MojoHandle invitation;
    CHECK_EQ(MOJO_RESULT_OK,
             MojoAcceptInvitation(&transport_endpoint, &options, &invitation));
    return invitation;
  }

  static MojoHandle ExtractPipeFromInvitation(MojoHandle invitation) {
    MojoHandle pipe = MOJO_HANDLE_INVALID;
    const uint32_t kPipeName = 0;
    EXPECT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                  invitation, &kPipeName, 4, nullptr, &pipe));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
    return pipe;
  }
};

#define DEFINE_TEST_CLIENT(name)                \
  class name##Impl : public TestClientBase {    \
   public:                                      \
    static void Run();                          \
  };                                            \
  MULTIPROCESS_TEST_MAIN(name) {                \
    name##Impl::Run();                          \
    return testing::Test::HasFailure() ? 1 : 0; \
  }                                             \
  void name##Impl::Run()

const std::string kTestMessage1 = "i am the pusher robot";
const std::string kTestMessage2 = "i push the messages down the pipe";
const std::string kTestMessage3 = "i am the shover robot";
const std::string kTestMessage4 = "i shove the messages down the pipe";

TEST_F(MAYBE_InvitationTest, SendInvitation) {
  MojoHandle primordial_pipe;
  base::Process child_process =
      LaunchChildTestClient("SendInvitationClient", &primordial_pipe, 1,
                            MOJO_SEND_INVITATION_FLAG_NONE);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  WaitForProcessToTerminate(child_process);
}

DEFINE_TEST_CLIENT(SendInvitationClient) {
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  MojoHandle primordial_pipe = ExtractPipeFromInvitation(invitation);

  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));
}

TEST_F(MAYBE_InvitationTest, SendInvitationMultiplePipes) {
  MojoHandle pipes[2];
  base::Process child_process =
      LaunchChildTestClient("SendInvitationMultiplePipesClient", pipes, 2,
                            MOJO_SEND_INVITATION_FLAG_NONE);

  WriteMessage(pipes[0], kTestMessage1);
  WriteMessage(pipes[1], kTestMessage2);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(pipes[0]));
  EXPECT_EQ(kTestMessage4, ReadMessage(pipes[1]));

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipes[0]));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipes[1]));

  WaitForProcessToTerminate(child_process);
}

DEFINE_TEST_CLIENT(SendInvitationMultiplePipesClient) {
  MojoHandle pipes[2];
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  const uint32_t pipe_names[] = {0, 1};
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoExtractMessagePipeFromInvitation(invitation, &pipe_names[0], 4,
                                                 nullptr, &pipes[0]));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoExtractMessagePipeFromInvitation(invitation, &pipe_names[1], 4,
                                                 nullptr, &pipes[1]));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_READABLE);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(pipes[0]));
  ASSERT_EQ(kTestMessage2, ReadMessage(pipes[1]));
  WriteMessage(pipes[0], kTestMessage3);
  WriteMessage(pipes[1], kTestMessage4);
  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipes[0]));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipes[1]));
}

const char kErrorMessage[] = "ur bad :(";
const char kDisconnectMessage[] = "go away plz";

class RemoteProcessState {
 public:
  RemoteProcessState()
      : callback_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  RemoteProcessState(const RemoteProcessState&) = delete;
  RemoteProcessState& operator=(const RemoteProcessState&) = delete;

  ~RemoteProcessState() = default;

  bool disconnected() {
    base::AutoLock lock(lock_);
    return disconnected_;
  }

  void set_error_callback(base::RepeatingClosure callback) {
    error_callback_ = std::move(callback);
  }

  void set_expected_error_message(const std::string& expected) {
    expected_error_message_ = expected;
  }

  void NotifyError(const std::string& error_message, bool disconnected) {
    base::AutoLock lock(lock_);
    CHECK(!disconnected_);
    EXPECT_TRUE(base::Contains(error_message, expected_error_message_));
    disconnected_ = disconnected;
    ++call_count_;
    if (error_callback_) {
      callback_task_runner_->PostTask(FROM_HERE, error_callback_);
    }
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  base::Lock lock_;
  int call_count_ = 0;
  bool disconnected_ = false;
  std::string expected_error_message_;
  base::RepeatingClosure error_callback_;
};

void TestProcessErrorHandler(uintptr_t context,
                             const MojoProcessErrorDetails* details) {
  auto* state = reinterpret_cast<RemoteProcessState*>(context);
  std::string error_message;
  if (details->error_message) {
    error_message =
        std::string(details->error_message, details->error_message_length - 1);
  }
  state->NotifyError(error_message,
                     details->flags & MOJO_PROCESS_ERROR_FLAG_DISCONNECTED);
}

TEST_F(MAYBE_InvitationTest, ProcessErrors) {
  RemoteProcessState process_state;
  MojoHandle pipe;
  base::Process child_process = LaunchChildTestClient(
      "ProcessErrorsClient", &pipe, 1, MOJO_SEND_INVITATION_FLAG_NONE,
      &TestProcessErrorHandler, reinterpret_cast<uintptr_t>(&process_state));

  MojoMessageHandle message;
  WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(pipe, nullptr, &message));

  base::RunLoop error_loop;
  process_state.set_error_callback(error_loop.QuitClosure());

  // Report this message as "bad". This should cause the error handler to be
  // invoked and the RunLoop to be quit.
  process_state.set_expected_error_message(kErrorMessage);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoNotifyBadMessage(message, kErrorMessage, sizeof(kErrorMessage),
                                 nullptr));
  error_loop.Run();
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));

  // Now tell the child it can exit, and wait for it to disconnect.
  base::RunLoop disconnect_loop;
  process_state.set_error_callback(disconnect_loop.QuitClosure());
  process_state.set_expected_error_message(std::string());
  WriteMessage(pipe, kDisconnectMessage);
  disconnect_loop.Run();

  EXPECT_TRUE(process_state.disconnected());

  WaitForProcessToTerminate(child_process);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe));
}

DEFINE_TEST_CLIENT(ProcessErrorsClient) {
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  MojoHandle pipe = ExtractPipeFromInvitation(invitation);

  // Send a message. Contents are irrelevant, the test process is just going to
  // flag it as a bad.
  WriteMessage(pipe, "doesn't matter");

  // Wait for our goodbye before exiting.
  WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kDisconnectMessage, ReadMessage(pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe));
}

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
// Temporary removed support for reinvitation for non-isolated connections.
TEST_F(MAYBE_InvitationTest, DISABLED_Reinvitation) {
  // The gist of this test is that a process should be able to accept an
  // invitation, lose its connection to the process network, and then accept a
  // new invitation to re-establish communication.

  // We pass an extra PlatformChannel endpoint to the child process which it
  // will use to accept a secondary invitation after we sever its first
  // connection.
  PlatformChannel secondary_channel;
  auto command_line = base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions launch_options;
  PrepareToPassRemoteEndpoint(&secondary_channel, &launch_options,
                              &command_line, kSecondaryChannelHandleSwitch);

  MojoHandle pipe;
  base::Process child_process = LaunchChildTestClient(
      "ReinvitationClient", &pipe, 1, MOJO_SEND_INVITATION_FLAG_NONE, nullptr,
      0, &command_line, &launch_options);
  secondary_channel.RemoteProcessLaunchAttempted();

  // Synchronize end-to-end communication first to ensure the process connection
  // is fully established.
  WriteMessage(pipe, kTestMessage1);
  EXPECT_EQ(kTestMessage2, ReadMessage(pipe));

  // Force-disconnect the child process.
  Core::Get()->GetNodeController()->ForceDisconnectProcessForTesting(
      child_process.Pid());

  // The above disconnection should force pipe closure eventually.
  WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  MojoClose(pipe);

  // Now use our secondary channel to send a new invitation to the same process.
  // It should be able to accept the new invitation and re-establish
  // communication.
  mojo::OutgoingInvitation new_invitation;
  auto new_pipe = new_invitation.AttachMessagePipe(0);
  mojo::OutgoingInvitation::Send(std::move(new_invitation),
                                 child_process.Handle(),
                                 secondary_channel.TakeLocalEndpoint());

  WriteMessage(new_pipe.get().value(), kTestMessage3);
  EXPECT_EQ(kTestMessage4, ReadMessage(new_pipe.get().value()));
  WriteMessage(new_pipe.get().value(), kDisconnectMessage);

  WaitForProcessToTerminate(child_process);
}
#endif  // BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)

DEFINE_TEST_CLIENT(ReinvitationClient) {
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  MojoHandle pipe = ExtractPipeFromInvitation(invitation);
  EXPECT_EQ(kTestMessage1, ReadMessage(pipe));
  WriteMessage(pipe, kTestMessage2);

  // Wait for the pipe to break due to forced process disconnection.
  WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  MojoClose(pipe);

  // Now grab the secondary channel and accept a new invitation from it.
  PlatformChannelEndpoint new_endpoint =
      PlatformChannel::RecoverPassedEndpointFromString(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kSecondaryChannelHandleSwitch));
  auto secondary_invitation =
      mojo::IncomingInvitation::Accept(std::move(new_endpoint));
  auto new_pipe = secondary_invitation.ExtractMessagePipe(0);

  // Ensure that the new connection is working end-to-end.
  EXPECT_EQ(kTestMessage3, ReadMessage(new_pipe.get().value()));
  WriteMessage(new_pipe.get().value(), kTestMessage4);
  EXPECT_EQ(kDisconnectMessage, ReadMessage(new_pipe.get().value()));
}

TEST_F(MAYBE_InvitationTest, SendIsolatedInvitation) {
  MojoHandle primordial_pipe;
  base::Process child_process =
      LaunchChildTestClient("SendIsolatedInvitationClient", &primordial_pipe, 1,
                            MOJO_SEND_INVITATION_FLAG_ISOLATED);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  WaitForProcessToTerminate(child_process);
}

DEFINE_TEST_CLIENT(SendIsolatedInvitationClient) {
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
  MojoHandle primordial_pipe = ExtractPipeFromInvitation(invitation);

  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));
}

TEST_F(MAYBE_InvitationTest, SendMultipleIsolatedInvitations) {
  if (mojo::core::IsMojoIpczEnabled()) {
    // This feature is not particularly useful in a world where isolated
    // connections are only supported between broker nodes.
    GTEST_SKIP() << "MojoIpcz does not support multiple isolated invitations "
                 << "between the same two nodes.";
  }

  // We send a secondary transport to the client process so we can send a second
  // isolated invitation.
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  PlatformChannel secondary_transport;
  base::LaunchOptions options;
  PrepareToPassRemoteEndpoint(&secondary_transport, &options, &command_line,
                              kSecondaryChannelHandleSwitch);

  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "SendMultipleIsolatedInvitationsClient", &primordial_pipe, 1,
      MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, &command_line, &options);
  secondary_transport.RemoteProcessLaunchAttempted();

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));

  // Send another invitation over our seconary pipe. This should trample the
  // original connection, breaking the first pipe.
  MojoHandle new_pipe;
  SendInvitationToClient(
      secondary_transport.TakeLocalEndpoint().TakePlatformHandle(),
      child_process.Handle(), &new_pipe, 1, MOJO_SEND_INVITATION_FLAG_ISOLATED,
      nullptr, 0, "");
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  // And the new pipe should be working.
  WriteMessage(new_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(new_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(new_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(new_pipe));

  WaitForProcessToTerminate(child_process);
}

DEFINE_TEST_CLIENT(SendMultipleIsolatedInvitationsClient) {
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
  MojoHandle primordial_pipe = ExtractPipeFromInvitation(invitation);

  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);

  // The above pipe should get closed once we accept a new invitation.
  invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED,
                                kSecondaryChannelHandleSwitch);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  primordial_pipe = MOJO_HANDLE_INVALID;
  const uint32_t pipe_name = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoExtractMessagePipeFromInvitation(invitation, &pipe_name, 4,
                                                 nullptr, &primordial_pipe));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));
}

TEST_F(MAYBE_InvitationTest, SendIsolatedInvitationWithDuplicateName) {
  if (mojo::core::IsMojoIpczEnabled()) {
    // This feature is not particularly useful in a world where isolated
    // connections are only supported between broker nodes.
    GTEST_SKIP() << "MojoIpcz does not support multiple isolated invitations "
                 << "between the same two nodes.";
  }

  PlatformChannel channel1;
  PlatformChannel channel2;
  MojoHandle pipe0, pipe1;
  const char kConnectionName[] = "there can be only one!";
  SendInvitationToClient(channel1.TakeLocalEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe0, 1,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0,
                         kConnectionName);

  // Send another invitation with the same connection name. |pipe0| should be
  // disconnected as the first invitation's connection is torn down.
  SendInvitationToClient(channel2.TakeLocalEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe1, 1,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0,
                         kConnectionName);

  WaitForSignals(pipe0, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

TEST_F(MAYBE_InvitationTest, SendIsolatedInvitationToSelf) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "MojoIpcz does not support nodes sending isolated "
                 << "invitations to themselves.";
  }

  PlatformChannel channel;
  MojoHandle pipe0, pipe1;
  SendInvitationToClient(channel.TakeLocalEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe0, 1,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, "");
  SendInvitationToClient(channel.TakeRemoteEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe1, 1,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, "");

  WriteMessage(pipe0, kTestMessage1);
  EXPECT_EQ(kTestMessage1, ReadMessage(pipe1));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

TEST_F(MAYBE_InvitationTest, BrokenInvitationTransportBreaksAttachedPipe) {
  MojoHandle primordial_pipe;
  base::Process child_process =
      LaunchChildTestClient("BrokenTransportClient", &primordial_pipe, 1,
                            MOJO_SEND_INVITATION_FLAG_NONE);

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  WaitForProcessToTerminate(child_process);
}

TEST_F(MAYBE_InvitationTest,
       BrokenIsolatedInvitationTransportBreaksAttachedPipe) {
  MojoHandle primordial_pipe;
  base::Process child_process =
      LaunchChildTestClient("BrokenTransportClient", &primordial_pipe, 1,
                            MOJO_SEND_INVITATION_FLAG_ISOLATED);

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  WaitForProcessToTerminate(child_process);
}

DEFINE_TEST_CLIENT(BrokenTransportClient) {
  // No-op. Exit immediately without accepting any invitation.
}

TEST_F(MAYBE_InvitationTest, NonBrokerToNonBroker) {
  // Tests a non-broker inviting another non-broker to join the network.
  MojoHandle host;
  base::Process host_process = LaunchChildTestClient(
      "NonBrokerToNonBrokerHost", &host, 1, MOJO_SEND_INVITATION_FLAG_NONE);

  // Send a pipe to the host, which it will forward to its launched client.
  MessagePipe pipe;
  MojoHandle client = pipe.handle0.release().value();
  MojoHandle pipe_for_client = pipe.handle1.release().value();
  WriteMessageWithHandles(host, "aaa", &pipe_for_client, 1);

  // If the host can successfully invite the client, the client will receive
  // this message and we'll eventually receive a message back from it.
  WriteMessage(client, "bbb");
  EXPECT_EQ("ccc", ReadMessage(client));

  // Signal to the host that it's OK to terminate, then wait for it ack.
  WriteMessage(host, "bye");
  WaitForProcessToTerminate(host_process);
  MojoClose(host);
  MojoClose(client);
}

DEFINE_TEST_CLIENT(NonBrokerToNonBrokerHost) {
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  MojoHandle test = ExtractPipeFromInvitation(invitation);

  MojoHandle pipe_for_client;
  EXPECT_EQ("aaa", ReadMessageWithHandles(test, &pipe_for_client, 1));

  MojoHandle client;
  base::Process client_process =
      LaunchChildTestClient("NonBrokerToNonBrokerClient", &client, 1,
                            MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);

  // Forward the pipe from the test to the client, then wait. We're done
  // whenever the client acks. The success of the test is determined by
  // the outcome of interactions between the test and the client process.
  WriteMessageWithHandles(client, "ddd", &pipe_for_client, 1);

  // Wait for a signal from the test to let us know we can terminate.
  EXPECT_EQ("bye", ReadMessage(test));
  WriteMessage(client, "bye");
  WaitForProcessToTerminate(client_process);

  MojoClose(client);
  MojoClose(test);
}

DEFINE_TEST_CLIENT(NonBrokerToNonBrokerClient) {
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_INHERIT_BROKER);
  MojoHandle host = ExtractPipeFromInvitation(invitation);

  MojoHandle pipe_to_test;
  EXPECT_EQ("ddd", ReadMessageWithHandles(host, &pipe_to_test, 1));

  EXPECT_EQ("bbb", ReadMessage(pipe_to_test));
  WriteMessage(pipe_to_test, "ccc");

  EXPECT_EQ("bye", ReadMessage(host));
  MojoClose(host);
  MojoClose(pipe_to_test);
}

TEST_F(MAYBE_InvitationTest, MultiBrokerNetwork) {
  // Regression test for https://crbug.com/1432382, ensuring that a non-broker
  // can communicate with brokers other than its own and can transmit platform
  // handles between them.

  if (!mojo::core::IsMojoIpczEnabled()) {
    // Mutli-broker networks are only supported with ipcz enabled.
    GTEST_SKIP() << "This tests functionality which is only supported when "
                 << "MojoIpcz is enabled, but MojoIpcz is not enabled.";
  }

  ASSERT_TRUE(mojo::core::GetIpczNodeOptions().is_broker);

  // First we launch a second broker and connect to it.
  MojoHandle secondary_broker;
  base::Process secondary_broker_process =
      LaunchChildTestClient("SecondaryBroker", &secondary_broker, 1,
                            MOJO_SEND_INVITATION_FLAG_ISOLATED);

  // Then launch a non-broker and connect to it.
  MojoHandle client;
  base::Process client_process = LaunchChildTestClient(
      "MultiBrokerNetworkClient", &client, 1, MOJO_SEND_INVITATION_FLAG_NONE);

  // Pass them each one end of the same pipe.
  MessagePipe pipe;
  MojoHandle broker_to_client = pipe.handle0.release().value();
  MojoHandle client_to_broker = pipe.handle1.release().value();
  WriteMessageWithHandles(secondary_broker, "hi", &broker_to_client, 1);
  WriteMessageWithHandles(client, "hi", &client_to_broker, 1);

  // Signal to the host that it's OK to terminate, then wait for acks.
  WriteMessage(secondary_broker, "bye");
  WriteMessage(client, "bye");
  WaitForProcessToTerminate(secondary_broker_process);
  WaitForProcessToTerminate(client_process);
  MojoClose(secondary_broker);
  MojoClose(client);
}

MojoHandle CreateMemory(std::string_view contents) {
  auto region = base::WritableSharedMemoryRegion::Create(contents.size());
  auto mapping = region.Map();
  memcpy(mapping.memory(), contents.data(), contents.size());
  auto buffer = WrapReadOnlySharedMemoryRegion(
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region)));
  return buffer.release().value();
}

std::string ReadMemory(MojoHandle handle) {
  auto region = UnwrapReadOnlySharedMemoryRegion(
      ScopedSharedBufferHandle{SharedBufferHandle{handle}});
  auto mapping = region.Map();
  std::string_view contents{reinterpret_cast<const char*>(mapping.memory()),
                            region.GetSize()};
  return std::string{contents};
}

constexpr size_t kNumMultiBrokerMessageIterations = 100;

DEFINE_TEST_CLIENT(SecondaryBroker) {
  ASSERT_TRUE(mojo::core::GetIpczNodeOptions().is_broker);
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
  MojoHandle test_runner = ExtractPipeFromInvitation(invitation);

  MojoHandle client;
  EXPECT_EQ("hi", ReadMessageWithHandles(test_runner, &client, 1));

  // Note that handle passing can succeed even if communication is broken
  // between non-brokers and secondary brokers, as long as no direct link
  // between them has been fully negotiated yet. We perform many iterations of
  // handle passing to ensure adequate coverage.
  for (size_t i = 0; i < kNumMultiBrokerMessageIterations; ++i) {
    MojoHandle buffer = CreateMemory("lol");
    WriteMessageWithHandles(client, "aaa", &buffer, 1);
    EXPECT_EQ("bbb", ReadMessageWithHandles(client, &buffer, 1));
    EXPECT_EQ("lmao", ReadMemory(buffer));
  }

  EXPECT_EQ("bye", ReadMessage(test_runner));

  MojoClose(test_runner);
  MojoClose(client);
}

DEFINE_TEST_CLIENT(MultiBrokerNetworkClient) {
  ASSERT_FALSE(mojo::core::GetIpczNodeOptions().is_broker);
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  MojoHandle test_runner = ExtractPipeFromInvitation(invitation);

  MojoHandle secondary_broker;
  EXPECT_EQ("hi", ReadMessageWithHandles(test_runner, &secondary_broker, 1));

  for (size_t i = 0; i < kNumMultiBrokerMessageIterations; ++i) {
    MojoHandle buffer = CreateMemory("lmao");
    WriteMessageWithHandles(secondary_broker, "bbb", &buffer, 1);
    EXPECT_EQ("aaa", ReadMessageWithHandles(secondary_broker, &buffer, 1));
    EXPECT_EQ("lol", ReadMemory(buffer));
  }

  EXPECT_EQ("bye", ReadMessage(test_runner));

  MojoClose(test_runner);
  MojoClose(secondary_broker);
}

}  // namespace
}  // namespace core
}  // namespace mojo
