// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_server.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_test_util.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "remoting/host/ipc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {
namespace {

using testing::_;
using testing::Return;

static const char kClientProcessName[] = "ClientProcess";
static const char kClientProcessServerNameSwitch[] =
    "chromoting-host-services-server-name";

MULTIPROCESS_TEST_MAIN(ClientProcess) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  mojo::PlatformChannelEndpoint endpoint =
      named_mojo_ipc_server::ConnectToServer(
          cmd_line->GetSwitchValueNative(kClientProcessServerNameSwitch));
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  auto message_pipe =
      invitation.ExtractMessagePipe(kChromotingHostServicesMessagePipeId);
  // |message_pipe| is always valid no matter whether the server sends an
  // invitation or not, so the best we can do is just to wait for the message
  // pipe to be closed by the server, which happens either when the server
  // refuses to send the invitation, or when the scoped message pipe is dropped
  // in BindChromotingHostServicesCallback.
  mojo::SimpleWatcher watcher(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  base::RunLoop run_loop;
  watcher.Watch(
      message_pipe.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult watch_result, const mojo::HandleSignalsState& state) {
            ASSERT_EQ(watch_result, MOJO_RESULT_OK);
            if (state.peer_closed()) {
              run_loop.Quit();
            }
          }));
  run_loop.Run();
  return 0;
}

}  // namespace

class ChromotingHostServicesServerTest : public testing::Test {
 public:
  ChromotingHostServicesServerTest();
  ~ChromotingHostServicesServerTest() override;

 protected:
  base::Process LaunchClientProcess();
  int WaitForProcessExit(base::Process& process);

  mojo::NamedPlatformChannel::ServerName server_name_ =
      named_mojo_ipc_server::test::GenerateRandomServerName();
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  // Helper thread to wait for process exit without blocking the main thread.
  base::Thread wait_for_process_exit_thread_{"wait_for_process_exit"};

  base::MockCallback<ChromotingHostServicesServer::Validator> mock_validator_;
  base::MockCallback<
      ChromotingHostServicesServer::BindChromotingHostServicesCallback>
      mock_bind_callback_;
  ChromotingHostServicesServer server_{server_name_, mock_validator_.Get(),
                                       mock_bind_callback_.Get()};
};

ChromotingHostServicesServerTest::ChromotingHostServicesServerTest() {
  wait_for_process_exit_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  server_.StartServer();
}

ChromotingHostServicesServerTest::~ChromotingHostServicesServerTest() {
  server_.StopServer();
  task_environment_.RunUntilIdle();
}

base::Process ChromotingHostServicesServerTest::LaunchClientProcess() {
  base::CommandLine cmd_line = base::GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchNative(kClientProcessServerNameSwitch, server_name_);
  return base::SpawnMultiProcessTestChild(kClientProcessName, cmd_line,
                                          /* options= */ {});
}

int ChromotingHostServicesServerTest::WaitForProcessExit(
    base::Process& process) {
  int exit_code;
  bool process_exited = false;
  base::RunLoop wait_for_process_exit_loop;
  wait_for_process_exit_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        process_exited = base::WaitForMultiprocessTestChildExit(
            process, TestTimeouts::action_timeout(), &exit_code);
      }),
      wait_for_process_exit_loop.QuitClosure());
  wait_for_process_exit_loop.Run();
  process.Close();
  EXPECT_TRUE(process_exited);
  return exit_code;
}

TEST_F(ChromotingHostServicesServerTest,
       ValidClientConnects_BindingCallbackIsCalled) {
  EXPECT_CALL(mock_validator_, Run(_)).WillOnce(Return(true));
  EXPECT_CALL(mock_bind_callback_, Run(_, _))
      .WillOnce(
          [&](mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
              base::ProcessId peer_pid) {
            ASSERT_TRUE(receiver.is_valid());
            ASSERT_NE(peer_pid, base::kNullProcessId);
          });

  base::Process client_process = LaunchClientProcess();
  WaitForProcessExit(client_process);
}

TEST_F(ChromotingHostServicesServerTest,
       InvalidClientConnects_BindingCallbackIsNotCalled) {
  EXPECT_CALL(mock_validator_, Run(_)).WillOnce(Return(false));
  EXPECT_CALL(mock_bind_callback_, Run(_, _)).Times(0);

  base::Process client_process = LaunchClientProcess();
  WaitForProcessExit(client_process);
}

}  // namespace remoting
