// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/system/invitation.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/test_switches.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
#include "mojo/public/cpp/platform/named_platform_channel.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/access_token.h"
#endif

namespace mojo {
namespace {

enum class InvitationType {
  kNormal,
  kIsolated,
#if BUILDFLAG(IS_WIN)
  // For now, the concept of an elevated process is only meaningful on Windows.
  kElevated,
#endif
};

enum class TransportType {
  kChannel,
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  // Fuchsia has no named pipe support.
  kChannelServer,
  // Test the scenario of calling SendIsolated without providing remote process
  // handle.
  kChannelServerWithoutHandle,
#endif
};

// Switches and values to tell clients of parameterized test runs what mode they
// should be testing against.
const char kTransportTypeSwitch[] = "test-transport-type";
const char kTransportTypeChannel[] = "channel";
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
const char kTransportTypeChannelServer[] = "channel-server";
#endif

// TODO(crbug.com/40900578): Flaky on Tsan.
#if defined(THREAD_SANITIZER)
#define MAYBE_InvitationCppTest DISABLED_InvitationCppTest
#else
#define MAYBE_InvitationCppTest InvitationCppTest
#endif
class MAYBE_InvitationCppTest
    : public testing::Test,
      public testing::WithParamInterface<TransportType> {
 public:
  MAYBE_InvitationCppTest() = default;

  MAYBE_InvitationCppTest(const MAYBE_InvitationCppTest&) = delete;
  MAYBE_InvitationCppTest& operator=(const MAYBE_InvitationCppTest&) = delete;

  ~MAYBE_InvitationCppTest() override = default;

 protected:
  void LaunchChildTestClient(const std::string& test_client_name,
                             ScopedMessagePipeHandle* primordial_pipes,
                             size_t num_primordial_pipes,
                             InvitationType invitation_type,
                             TransportType transport_type,
                             const ProcessErrorCallback& error_callback = {}) {
    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine());

    base::LaunchOptions launch_options;
    std::optional<PlatformChannel> channel;
    PlatformChannelEndpoint channel_endpoint;
    PlatformChannelServerEndpoint server_endpoint;
    switch (transport_type) {
      case TransportType::kChannel: {
        command_line.AppendSwitchASCII(kTransportTypeSwitch,
                                       kTransportTypeChannel);
        channel.emplace();
        channel->PrepareToPassRemoteEndpoint(&launch_options, &command_line);
#if BUILDFLAG(IS_WIN)
        launch_options.start_hidden = true;
#endif
        channel_endpoint = channel->TakeLocalEndpoint();
        break;
      }
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
      case TransportType::kChannelServer:
      case TransportType::kChannelServerWithoutHandle: {
        command_line.AppendSwitchASCII(kTransportTypeSwitch,
                                       kTransportTypeChannelServer);
        NamedPlatformChannel::Options named_channel_options;
#if !BUILDFLAG(IS_WIN)
        CHECK(base::PathService::Get(base::DIR_TEMP,
                                     &named_channel_options.socket_dir));
#endif
        NamedPlatformChannel named_channel(named_channel_options);
        named_channel.PassServerNameOnCommandLine(&command_line);
        server_endpoint = named_channel.TakeServerEndpoint();
        break;
      }
#endif  //  !BUILDFLAG(IS_FUCHSIA)
    }

    std::string enable_features;
    std::string disable_features;
    base::FeatureList::GetInstance()->GetCommandLineFeatureOverrides(
        &enable_features, &disable_features);
    command_line.AppendSwitchASCII(switches::kEnableFeatures, enable_features);
    command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                   disable_features);
    if (invitation_type == InvitationType::kIsolated) {
      command_line.AppendSwitch(test_switches::kMojoIsBroker);
    }

    child_process_ = base::SpawnMultiProcessTestChild(
        test_client_name, command_line, launch_options);
    if (channel)
      channel->RemoteProcessLaunchAttempted();

    OutgoingInvitation invitation;
    if (invitation_type != InvitationType::kIsolated) {
      for (uint64_t name = 0; name < num_primordial_pipes; ++name)
        primordial_pipes[name] = invitation.AttachMessagePipe(name);
    }

#if BUILDFLAG(IS_WIN)
    if (invitation_type == InvitationType::kElevated) {
      // We can't elevate the child process because of UAC, so instead we just
      // lower the integrity level on the IO thread, so that OpenProcess() will
      // fail with access denied error on the server side, forcing the client
      // to be responsible for handle duplication. This trick works regardless
      // of whether the current process is elevated.
      core::GetIOTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&LowerCurrentThreadIntegrityLevel));

      invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
    }
#endif

    switch (transport_type) {
      case TransportType::kChannel:
        DCHECK(channel_endpoint.is_valid());
        if (invitation_type != InvitationType::kIsolated) {
          OutgoingInvitation::Send(std::move(invitation),
                                   child_process_.Handle(),
                                   std::move(channel_endpoint), error_callback);
        } else {
          DCHECK(primordial_pipes);
          DCHECK_EQ(num_primordial_pipes, 1u);
          primordial_pipes[0] = OutgoingInvitation::SendIsolated(
              std::move(channel_endpoint), {}, child_process_.Handle());
        }
        break;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
      case TransportType::kChannelServer:
        DCHECK(server_endpoint.is_valid());
        if (invitation_type != InvitationType::kIsolated) {
          OutgoingInvitation::Send(std::move(invitation),
                                   child_process_.Handle(),
                                   std::move(server_endpoint), error_callback);
        } else {
          DCHECK(primordial_pipes);
          DCHECK_EQ(num_primordial_pipes, 1u);
          // Provide the remote process handle when calling SendIsolated
          // function.
          primordial_pipes[0] = OutgoingInvitation::SendIsolated(
              std::move(server_endpoint), {}, child_process_.Handle());
        }
        break;
      case TransportType::kChannelServerWithoutHandle:
        DCHECK(server_endpoint.is_valid());
        if (invitation_type != InvitationType::kIsolated) {
          OutgoingInvitation::Send(std::move(invitation), {},
                                   std::move(server_endpoint), error_callback);
        } else {
          DCHECK(primordial_pipes);
          DCHECK_EQ(num_primordial_pipes, 1u);
          // Don't provide the remote process handle when calling SendIsolated
          // function.
          primordial_pipes[0] =
              OutgoingInvitation::SendIsolated(std::move(server_endpoint), {});
        }
        break;
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    }
  }

  void WaitForChildExit() {
    int wait_result = -1;
    base::WaitForMultiprocessTestChildExit(
        child_process_, TestTimeouts::action_timeout(), &wait_result);
    child_process_.Close();
    EXPECT_EQ(0, wait_result);
  }

  static void WriteMessage(const ScopedMessagePipeHandle& pipe,
                           std::string_view message) {
    CHECK_EQ(MOJO_RESULT_OK,
             WriteMessageRaw(pipe.get(), message.data(), message.size(),
                             nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  static std::string ReadMessage(const ScopedMessagePipeHandle& pipe) {
    CHECK_EQ(MOJO_RESULT_OK, Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE));

    std::vector<uint8_t> payload;
    std::vector<ScopedHandle> handles;
    CHECK_EQ(MOJO_RESULT_OK, ReadMessageRaw(pipe.get(), &payload, &handles,
                                            MOJO_READ_MESSAGE_FLAG_NONE));
    return std::string(payload.begin(), payload.end());
  }

#if BUILDFLAG(IS_WIN)
  static void LowerCurrentThreadIntegrityLevel() {
    auto restricted_access_token = base::win::AccessToken::FromCurrentProcess(
        /* impersonation= */ true, TOKEN_ALL_ACCESS);
    PCHECK(restricted_access_token);
    CHECK(restricted_access_token->IsImpersonation());
    CHECK_GT(restricted_access_token->IntegrityLevel(),
             static_cast<DWORD>(SECURITY_MANDATORY_UNTRUSTED_RID))
        << "Current integrity level must be higher than UNTRUSTED.";
    PCHECK(restricted_access_token->SetIntegrityLevel(
        SECURITY_MANDATORY_UNTRUSTED_RID));
    PCHECK(ImpersonateLoggedOnUser(restricted_access_token->get()));
  }
#endif

 private:
  base::test::TaskEnvironment task_environment_;
  base::Process child_process_;
};

class TestClientBase : public MAYBE_InvitationCppTest {
 public:
  TestClientBase(const TestClientBase&) = delete;
  TestClientBase& operator=(const TestClientBase&) = delete;

  static PlatformChannelEndpoint RecoverEndpointFromCommandLine() {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    std::string transport_type_string =
        command_line.GetSwitchValueASCII(kTransportTypeSwitch);
    CHECK(!transport_type_string.empty());
    if (transport_type_string != kTransportTypeChannel) {
      return NamedPlatformChannel::ConnectToServer(command_line);
    }
#endif
    return PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  }

  static IncomingInvitation AcceptInvitation(
      MojoAcceptInvitationFlags flags = MOJO_ACCEPT_INVITATION_FLAG_NONE) {
    return IncomingInvitation::Accept(RecoverEndpointFromCommandLine(), flags);
  }

  static ScopedMessagePipeHandle AcceptIsolatedInvitation() {
    return IncomingInvitation::AcceptIsolated(RecoverEndpointFromCommandLine());
  }
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

const char kTestMessage1[] = "hello";
const char kTestMessage2[] = "hello";

TEST_P(MAYBE_InvitationCppTest, Send) {
  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient("CppSendClient", &pipe, 1, InvitationType::kNormal,
                        GetParam());
  WriteMessage(pipe, kTestMessage1);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendClient) {
  auto invitation = AcceptInvitation();
  auto pipe = invitation.ExtractMessagePipe(0);
  CHECK_EQ(kTestMessage1, ReadMessage(pipe));
}

TEST_P(MAYBE_InvitationCppTest, SendIsolated) {
  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient("CppSendIsolatedClient", &pipe, 1,
                        InvitationType::kIsolated, GetParam());
  WriteMessage(pipe, kTestMessage1);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendIsolatedClient) {
  auto pipe = AcceptIsolatedInvitation();
  CHECK_EQ(kTestMessage1, ReadMessage(pipe));
}

#if BUILDFLAG(IS_WIN)
TEST_P(MAYBE_InvitationCppTest, SendElevated) {
  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient("CppSendElevatedClient", &pipe, 1,
                        InvitationType::kElevated, GetParam());
  WriteMessage(pipe, kTestMessage1);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendElevatedClient) {
  auto invitation = AcceptInvitation(MOJO_ACCEPT_INVITATION_FLAG_ELEVATED);
  auto pipe = invitation.ExtractMessagePipe(0);
  CHECK_EQ(kTestMessage1, ReadMessage(pipe));
}
#endif  // BUILDFLAG(IS_WIN)

TEST_P(MAYBE_InvitationCppTest, SendWithMultiplePipes) {
  ScopedMessagePipeHandle pipes[2];
  LaunchChildTestClient("CppSendWithMultiplePipesClient", pipes, 2,
                        InvitationType::kNormal, GetParam());
  WriteMessage(pipes[0], kTestMessage1);
  WriteMessage(pipes[1], kTestMessage2);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendWithMultiplePipesClient) {
  auto invitation = AcceptInvitation();
  auto pipe0 = invitation.ExtractMessagePipe(0);
  auto pipe1 = invitation.ExtractMessagePipe(1);
  CHECK_EQ(kTestMessage1, ReadMessage(pipe0));
  CHECK_EQ(kTestMessage2, ReadMessage(pipe1));
}

TEST(MAYBE_InvitationCppTest_NoParam, SendIsolatedInvitationWithDuplicateName) {
  if (mojo::core::IsMojoIpczEnabled()) {
    // This feature is not particularly useful in a world where isolated
    // connections are only supported between broker nodes.
    GTEST_SKIP() << "MojoIpcz does not support multiple isolated invitations "
                 << "between the same two nodes.";
  }

  base::test::TaskEnvironment task_environment;
  PlatformChannel channel1;
  PlatformChannel channel2;
  const char kConnectionName[] = "foo";
  ScopedMessagePipeHandle pipe0 = OutgoingInvitation::SendIsolated(
      channel1.TakeLocalEndpoint(), kConnectionName);
  ScopedMessagePipeHandle pipe1 = OutgoingInvitation::SendIsolated(
      channel2.TakeLocalEndpoint(), kConnectionName);
  Wait(pipe0.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED);
}

const char kErrorMessage[] = "ur bad :{{";
const char kDisconnectMessage[] = "go away plz";

// Flakily times out on Android under ASAN.
// crbug.com/1011494
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_ProcessErrors DISABLED_ProcessErrors
#else
#define MAYBE_ProcessErrors ProcessErrors
#endif

TEST_P(MAYBE_InvitationCppTest, MAYBE_ProcessErrors) {
  ProcessErrorCallback actual_error_callback;

  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient(
      "CppProcessErrorsClient", &pipe, 1, InvitationType::kNormal, GetParam(),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        ASSERT_TRUE(actual_error_callback);
        actual_error_callback.Run(error_message);
      }));

  MojoMessageHandle message;
  Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(pipe.get().value(), nullptr, &message));

  // Report the message as bad and expect to be notified through the process
  // error callback.
  base::RunLoop error_loop;
  actual_error_callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        EXPECT_TRUE(base::Contains(error_message, kErrorMessage));
        error_loop.Quit();
      });
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoNotifyBadMessage(message, kErrorMessage, sizeof(kErrorMessage),
                                 nullptr));
  error_loop.Run();
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));

  // TODO(crbug.com/40578072): Once we can rework the C++ invitation API
  // to also notify on disconnect, this test should cover that too. For now we
  // just tell the process to exit and wait for it to do.
  WriteMessage(pipe, kDisconnectMessage);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppProcessErrorsClient) {
  auto invitation = AcceptInvitation();
  auto pipe = invitation.ExtractMessagePipe(0);
  WriteMessage(pipe, "ignored");
  EXPECT_EQ(kDisconnectMessage, ReadMessage(pipe));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MAYBE_InvitationCppTest,
    testing::Values(TransportType::kChannel
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
                    ,
                    TransportType::kChannelServer,
                    TransportType::kChannelServerWithoutHandle
#endif
                    ));

}  // namespace
}  // namespace mojo
