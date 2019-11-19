// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/core/core.h"
#include "mojo/core/node_controller.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {
namespace core {
namespace {

enum class TransportType {
  kChannel,
  kChannelServer,
};

const char kSecondaryChannelHandleSwitch[] = "test-secondary-channel-handle";

class InvitationTest : public test::MojoTestBase {
 public:
  InvitationTest() = default;
  ~InvitationTest() override = default;

 protected:
  static base::Process LaunchChildTestClient(
      const std::string& test_client_name,
      MojoHandle* primordial_pipes,
      size_t num_primordial_pipes,
      TransportType transport_type,
      MojoSendInvitationFlags send_flags,
      MojoProcessErrorHandler error_handler = nullptr,
      uintptr_t error_handler_context = 0,
      base::CommandLine* custom_command_line = nullptr,
      base::LaunchOptions* custom_launch_options = nullptr);

  static void SendInvitationToClient(
      PlatformHandle endpoint_handle,
      base::ProcessHandle process,
      MojoHandle* primordial_pipes,
      size_t num_primordial_pipes,
      TransportType transport_type,
      MojoSendInvitationFlags flags,
      MojoProcessErrorHandler error_handler,
      uintptr_t error_handler_context,
      base::StringPiece isolated_invitation_name);

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(InvitationTest);
};

void PrepareToPassRemoteEndpoint(PlatformChannel* channel,
                                 base::LaunchOptions* options,
                                 base::CommandLine* command_line,
                                 base::StringPiece switch_name = {}) {
  std::string value;
#if defined(OS_FUCHSIA)
  channel->PrepareToPassRemoteEndpoint(&options->handles_to_transfer, &value);
#elif defined(OS_MACOSX)
  channel->PrepareToPassRemoteEndpoint(&options->mach_ports_for_rendezvous,
                                       &value);
#elif defined(OS_POSIX)
  channel->PrepareToPassRemoteEndpoint(&options->fds_to_remap, &value);
#elif defined(OS_WIN)
  channel->PrepareToPassRemoteEndpoint(&options->handles_to_inherit, &value);
#else
#error "Platform not yet supported."
#endif

  if (switch_name.empty())
    switch_name = PlatformChannel::kHandleSwitch;
  command_line->AppendSwitchASCII(switch_name.as_string(), value);
}

TEST_F(InvitationTest, Create) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  MojoCreateInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_INVITATION_FLAG_NONE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(&options, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
}

TEST_F(InvitationTest, InvalidArguments) {
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

TEST_F(InvitationTest, AttachAndExtractLocally) {
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

TEST_F(InvitationTest, ClosedInvitationClosesAttachments) {
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

TEST_F(InvitationTest, AttachNameInUse) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAttachMessagePipeToInvitation(
                                invitation, "x", 1, nullptr, &pipe0));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe0);

  MojoHandle pipe1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      MojoAttachMessagePipeToInvitation(invitation, "x", 1, nullptr, &pipe1));
  EXPECT_EQ(MOJO_HANDLE_INVALID, pipe1);
  EXPECT_EQ(MOJO_RESULT_OK, MojoAttachMessagePipeToInvitation(
                                invitation, "y", 1, nullptr, &pipe1));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe1);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

// static
base::Process InvitationTest::LaunchChildTestClient(
    const std::string& test_client_name,
    MojoHandle* primordial_pipes,
    size_t num_primordial_pipes,
    TransportType transport_type,
    MojoSendInvitationFlags send_flags,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    base::CommandLine* custom_command_line,
    base::LaunchOptions* custom_launch_options) {
  base::CommandLine default_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::CommandLine& command_line =
      custom_command_line ? *custom_command_line : default_command_line;

  base::LaunchOptions default_launch_options;
  base::LaunchOptions& launch_options =
      custom_launch_options ? *custom_launch_options : default_launch_options;
#if defined(OS_WIN)
  launch_options.start_hidden = true;
#endif

#if !defined(OS_FUCHSIA)
  base::Optional<NamedPlatformChannel> named_channel;
#endif
  base::Optional<PlatformChannel> channel;
  PlatformHandle local_endpoint_handle;
  if (transport_type == TransportType::kChannel) {
    channel.emplace();
    PrepareToPassRemoteEndpoint(&channel.value(), &launch_options,
                                &command_line);
    local_endpoint_handle = channel->TakeLocalEndpoint().TakePlatformHandle();
  } else {
#if !defined(OS_FUCHSIA)
    NamedPlatformChannel::Options named_channel_options;
#if !defined(OS_WIN)
    CHECK(base::PathService::Get(base::DIR_TEMP,
                                 &named_channel_options.socket_dir));
#endif
    named_channel.emplace(named_channel_options);
    named_channel->PassServerNameOnCommandLine(&command_line);
    local_endpoint_handle =
        named_channel->TakeServerEndpoint().TakePlatformHandle();
#else   //  !defined(OS_FUCHSIA)
    NOTREACHED() << "Named pipe support does not exist for Mojo on Fuchsia.";
#endif  //  !defined(OS_FUCHSIA)
  }

  base::Process child_process = base::SpawnMultiProcessTestChild(
      test_client_name, command_line, launch_options);
  if (channel)
    channel->RemoteProcessLaunchAttempted();

  SendInvitationToClient(std::move(local_endpoint_handle),
                         child_process.Handle(), primordial_pipes,
                         num_primordial_pipes, transport_type, send_flags,
                         error_handler, error_handler_context, "");

  return child_process;
}

// static
void InvitationTest::SendInvitationToClient(
    PlatformHandle endpoint_handle,
    base::ProcessHandle process,
    MojoHandle* primordial_pipes,
    size_t num_primordial_pipes,
    TransportType transport_type,
    MojoSendInvitationFlags flags,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    base::StringPiece isolated_invitation_name) {
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
#if defined(OS_WIN)
  process_handle.value =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(process));
#else
  process_handle.value = static_cast<uint64_t>(process);
#endif

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  if (transport_type == TransportType::kChannel)
    transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  else
    transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER;
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

class TestClientBase : public InvitationTest {
 public:
  static MojoHandle AcceptInvitation(MojoAcceptInvitationFlags flags,
                                     base::StringPiece switch_name = {}) {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
    PlatformChannelEndpoint channel_endpoint;
#if !defined(OS_FUCHSIA)
    channel_endpoint = NamedPlatformChannel::ConnectToServer(command_line);
#endif
    if (!channel_endpoint.is_valid()) {
      if (switch_name.empty()) {
        channel_endpoint =
            PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
      } else {
        channel_endpoint = PlatformChannel::RecoverPassedEndpointFromString(
            command_line.GetSwitchValueASCII(switch_name));
      }
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClientBase);
};

#define DEFINE_TEST_CLIENT(name)             \
  class name##Impl : public TestClientBase { \
   public:                                   \
    static void Run();                       \
  };                                         \
  MULTIPROCESS_TEST_MAIN(name) {             \
    name##Impl::Run();                       \
    return 0;                                \
  }                                          \
  void name##Impl::Run()

const std::string kTestMessage1 = "i am the pusher robot";
const std::string kTestMessage2 = "i push the messages down the pipe";
const std::string kTestMessage3 = "i am the shover robot";
const std::string kTestMessage4 = "i shove the messages down the pipe";

TEST_F(InvitationTest, SendInvitation) {
  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "SendInvitationClient", &primordial_pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_NONE);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendInvitationClient) {
  MojoHandle primordial_pipe;
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
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

TEST_F(InvitationTest, SendInvitationMultiplePipes) {
  MojoHandle pipes[2];
  base::Process child_process = LaunchChildTestClient(
      "SendInvitationMultiplePipesClient", pipes, 2, TransportType::kChannel,
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

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
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

  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_READABLE);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(pipes[0]));
  ASSERT_EQ(kTestMessage2, ReadMessage(pipes[1]));
  WriteMessage(pipes[0], kTestMessage3);
  WriteMessage(pipes[1], kTestMessage4);
  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
}

#if !defined(OS_FUCHSIA)
TEST_F(InvitationTest, SendInvitationWithServer) {
  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "SendInvitationWithServerClient", &primordial_pipe, 1,
      TransportType::kChannelServer, MOJO_SEND_INVITATION_FLAG_NONE);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendInvitationWithServerClient) {
  MojoHandle primordial_pipe;
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
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
#endif  // !defined(OS_FUCHSIA)

const char kErrorMessage[] = "ur bad :(";
const char kDisconnectMessage[] = "go away plz";

class RemoteProcessState {
 public:
  RemoteProcessState()
      : callback_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
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
    EXPECT_NE(error_message.find(expected_error_message_), std::string::npos);
    disconnected_ = disconnected;
    ++call_count_;
    if (error_callback_)
      callback_task_runner_->PostTask(FROM_HERE, error_callback_);
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  base::Lock lock_;
  int call_count_ = 0;
  bool disconnected_ = false;
  std::string expected_error_message_;
  base::RepeatingClosure error_callback_;

  DISALLOW_COPY_AND_ASSIGN(RemoteProcessState);
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

TEST_F(InvitationTest, ProcessErrors) {
  RemoteProcessState process_state;
  MojoHandle pipe;
  base::Process child_process = LaunchChildTestClient(
      "ProcessErrorsClient", &pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_NONE, &TestProcessErrorHandler,
      reinterpret_cast<uintptr_t>(&process_state));

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

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(ProcessErrorsClient) {
  MojoHandle pipe;
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  const uint32_t pipe_name = 0;
  ASSERT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, &pipe_name, 4, nullptr, &pipe));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  // Send a message. Contents are irrelevant, the test process is just going to
  // flag it as a bad.
  WriteMessage(pipe, "doesn't matter");

  // Wait for our goodbye before exiting.
  WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kDisconnectMessage, ReadMessage(pipe));
}

TEST_F(InvitationTest, Reinvitation) {
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
      "ReinvitationClient", &pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_NONE, nullptr, 0, &command_line,
      &launch_options);
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

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(ReinvitationClient) {
  MojoHandle pipe;
  MojoHandle invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_NONE);
  const uint32_t pipe_name = 0;
  ASSERT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, &pipe_name, 4, nullptr, &pipe));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
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

TEST_F(InvitationTest, SendIsolatedInvitation) {
  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "SendIsolatedInvitationClient", &primordial_pipe, 1,
      TransportType::kChannel, MOJO_SEND_INVITATION_FLAG_ISOLATED);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendIsolatedInvitationClient) {
  MojoHandle primordial_pipe;
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
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

TEST_F(InvitationTest, SendMultipleIsolatedInvitations) {
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
      TransportType::kChannel, MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0,
      &command_line, &options);
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
      child_process.Handle(), &new_pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, "");
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  // And the new pipe should be working.
  WriteMessage(new_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(new_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(new_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(new_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendMultipleIsolatedInvitationsClient) {
  MojoHandle primordial_pipe;
  MojoHandle invitation =
      AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED);
  const uint32_t pipe_name = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoExtractMessagePipeFromInvitation(invitation, &pipe_name, 4,
                                                 nullptr, &primordial_pipe));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);

  // The above pipe should get closed once we accept a new invitation.
  invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ISOLATED,
                                kSecondaryChannelHandleSwitch);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  primordial_pipe = MOJO_HANDLE_INVALID;
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

TEST_F(InvitationTest, SendIsolatedInvitationWithDuplicateName) {
  PlatformChannel channel1;
  PlatformChannel channel2;
  MojoHandle pipe0, pipe1;
  const char kConnectionName[] = "there can be only one!";
  SendInvitationToClient(
      channel1.TakeLocalEndpoint().TakePlatformHandle(),
      base::kNullProcessHandle, &pipe0, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, kConnectionName);

  // Send another invitation with the same connection name. |pipe0| should be
  // disconnected as the first invitation's connection is torn down.
  SendInvitationToClient(
      channel2.TakeLocalEndpoint().TakePlatformHandle(),
      base::kNullProcessHandle, &pipe1, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, kConnectionName);

  WaitForSignals(pipe0, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
}

TEST_F(InvitationTest, SendIsolatedInvitationToSelf) {
  PlatformChannel channel;
  MojoHandle pipe0, pipe1;
  SendInvitationToClient(channel.TakeLocalEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe0, 1,
                         TransportType::kChannel,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, "");
  SendInvitationToClient(channel.TakeRemoteEndpoint().TakePlatformHandle(),
                         base::kNullProcessHandle, &pipe1, 1,
                         TransportType::kChannel,
                         MOJO_SEND_INVITATION_FLAG_ISOLATED, nullptr, 0, "");

  WriteMessage(pipe0, kTestMessage1);
  EXPECT_EQ(kTestMessage1, ReadMessage(pipe1));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

TEST_F(InvitationTest, BrokenInvitationTransportBreaksAttachedPipe) {
  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "BrokenTransportClient", &primordial_pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_NONE);

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

TEST_F(InvitationTest, BrokenIsolatedInvitationTransportBreaksAttachedPipe) {
  MojoHandle primordial_pipe;
  base::Process child_process = LaunchChildTestClient(
      "BrokenTransportClient", &primordial_pipe, 1, TransportType::kChannel,
      MOJO_SEND_INVITATION_FLAG_ISOLATED);

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(BrokenTransportClient) {
  // No-op. Exit immediately without accepting any invitation.
}

}  // namespace
}  // namespace core
}  // namespace mojo
