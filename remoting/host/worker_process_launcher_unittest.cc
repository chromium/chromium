// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/worker_process_launcher.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;
using testing::AnyNumber;
using testing::Expectation;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace remoting {

namespace {

class MockProcessLauncherDelegate : public WorkerProcessLauncher::Delegate {
 public:
  MockProcessLauncherDelegate() {}

  MockProcessLauncherDelegate(const MockProcessLauncherDelegate&) = delete;
  MockProcessLauncherDelegate& operator=(const MockProcessLauncherDelegate&) =
      delete;

  ~MockProcessLauncherDelegate() override {}

  // WorkerProcessLauncher::Delegate interface.
  MOCK_METHOD(void, LaunchProcess, (WorkerProcessLauncher*), (override));
  MOCK_METHOD(void,
              GetRemoteAssociatedInterface,
              (mojo::GenericPendingAssociatedReceiver),
              (override));
  MOCK_METHOD(void, CloseChannel, (), (override));
  MOCK_METHOD(void, CrashProcess, (const base::Location&), (override));
  MOCK_METHOD(void, KillProcess, (), (override));
};

class MockIpcDelegate : public WorkerProcessIpcDelegate {
 public:
  MockIpcDelegate() {}

  MockIpcDelegate(const MockIpcDelegate&) = delete;
  MockIpcDelegate& operator=(const MockIpcDelegate&) = delete;

  ~MockIpcDelegate() override {}

  // WorkerProcessIpcDelegate interface.
  MOCK_METHOD(void, OnChannelConnected, (int32_t), (override));
  MOCK_METHOD(void, OnPermanentError, (int), (override));
  MOCK_METHOD(void, OnWorkerProcessStopped, (), (override));
  MOCK_METHOD(void,
              OnAssociatedInterfaceRequest,
              (const std::string& interface_name,
               mojo::ScopedInterfaceEndpointHandle handle),
              (override));
};

class MockWorkerListener : public IPC::Listener,
                           public mojom::WorkerProcessControl {
 public:
  MockWorkerListener() {}

  MockWorkerListener(const MockWorkerListener&) = delete;
  MockWorkerListener& operator=(const MockWorkerListener&) = delete;

  ~MockWorkerListener() override {}

  // mojom::WorkerProcessControl mock.
  MOCK_METHOD(void,
              CrashProcess,
              (const std::string&, const std::string&, int),
              (override));

  // IPC::Listener implementation.
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

 private:
  mojo::AssociatedReceiver<mojom::WorkerProcessControl> worker_process_control_{
      this};
};

void MockWorkerListener::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  ASSERT_EQ(interface_name, mojom::WorkerProcessControl::Name_);
  mojo::PendingAssociatedReceiver<mojom::WorkerProcessControl> pending_receiver(
      std::move(handle));
  worker_process_control_.Bind(std::move(pending_receiver));
  // Reset the receiver when the channel is closed to prevent any additional
  // tasks from being posted. If we don't reset the receiver, many of the tests
  // will appear to hang while waiting for the task runner to exit.
  worker_process_control_.reset_on_disconnect();
}

}  // namespace

class WorkerProcessLauncherTest : public testing::Test, public IPC::Listener {
 public:
  WorkerProcessLauncherTest();
  ~WorkerProcessLauncherTest() override;

  void SetUp() override;

  // IPC::Listener implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // WorkerProcessLauncher::Delegate mocks
  void LaunchProcess(WorkerProcessLauncher* event_handler);
  void LaunchProcessAndConnect(WorkerProcessLauncher* event_handler);
  void FailLaunchAndStopWorker(WorkerProcessLauncher* event_handler);
  void CrashProcess(const base::Location& location);
  void KillProcess();

  void TerminateWorker(int exit_code);

  // Connects the client end of the channel (the worker process's end).
  void ConnectClient();

  // Disconnects the client end of the channel.
  void DisconnectClient();

  // Disconnects the server end of the channel (the launcher's end).
  void DisconnectServer();

  // Sends a fake message to the launcher.
  void SendFakeMessageToLauncher();

  // Requests the worker to crash.
  void CrashWorker();

  // Starts the worker.
  void StartWorker();

  // Stops the worker.
  void StopWorker();

  // Quits |message_loop_|.
  void QuitMainMessageLoop();

  void Run() { loop_.Run(); }

 protected:
  void DoLaunchProcess();

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Receives messages sent to the worker process.
  MockWorkerListener client_listener_;

  // Receives messages sent from the worker process.
  MockIpcDelegate server_listener_;

  // Implements WorkerProcessLauncher::Delegate.
  std::unique_ptr<MockProcessLauncherDelegate> launcher_delegate_;

  // The client handle to the channel.
  mojo::ScopedMessagePipeHandle client_channel_handle_;

  // Client and server ends of the IPC channel.
  std::unique_ptr<IPC::ChannelProxy> channel_client_;
  std::unique_ptr<IPC::ChannelProxy> channel_server_;

  raw_ptr<WorkerProcessLauncher> event_handler_;

  // The worker process launcher.
  std::unique_ptr<WorkerProcessLauncher> launcher_;

  mojo::AssociatedRemote<mojom::DesktopSessionStateHandler>
      desktop_session_state_handler_;
  mojo::AssociatedRemote<mojom::WorkerProcessControl> worker_process_control_;

  // The launched worker process.
  base::Process worker_process_;

  // The internal run loop, used for managing messages.
  base::RunLoop loop_;
};

WorkerProcessLauncherTest::WorkerProcessLauncherTest()
    : event_handler_(nullptr) {}

WorkerProcessLauncherTest::~WorkerProcessLauncherTest() {}

void WorkerProcessLauncherTest::SetUp() {
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(),
      base::BindOnce(&WorkerProcessLauncherTest::QuitMainMessageLoop,
                     base::Unretained(this)));

  // Set up process launcher delegate.
  launcher_delegate_ = std::make_unique<MockProcessLauncherDelegate>();
  EXPECT_CALL(*launcher_delegate_, CloseChannel())
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::DisconnectServer));
  EXPECT_CALL(*launcher_delegate_, CrashProcess(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::CrashProcess));
  EXPECT_CALL(*launcher_delegate_, KillProcess())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::KillProcess));
}

void WorkerProcessLauncherTest::OnChannelConnected(int32_t peer_pid) {
  event_handler_->OnChannelConnected(peer_pid);
}

void WorkerProcessLauncherTest::OnChannelError() {
  event_handler_->OnChannelError();
}

void WorkerProcessLauncherTest::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  EXPECT_FALSE(event_handler_);
  event_handler_ = event_handler;

  DoLaunchProcess();
}

void WorkerProcessLauncherTest::LaunchProcessAndConnect(
    WorkerProcessLauncher* event_handler) {
  EXPECT_FALSE(event_handler_);
  event_handler_ = event_handler;

  DoLaunchProcess();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WorkerProcessLauncherTest::ConnectClient,
                                base::Unretained(this)));
}

void WorkerProcessLauncherTest::FailLaunchAndStopWorker(
    WorkerProcessLauncher* event_handler) {
  EXPECT_FALSE(event_handler_);

  event_handler->OnFatalError();

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WorkerProcessLauncherTest::StopWorker,
                                        base::Unretained(this)));
}

void WorkerProcessLauncherTest::CrashProcess(const base::Location& location) {
  worker_process_control_->CrashProcess(
      location.function_name(), location.file_name(), location.line_number());
}

void WorkerProcessLauncherTest::KillProcess() {
  event_handler_ = nullptr;

  DisconnectClient();
  TerminateWorker(kSuccessExitCode);
}

void WorkerProcessLauncherTest::TerminateWorker(int exit_code) {
  if (!worker_process_.IsValid()) {
    return;
  }
  ASSERT_TRUE(worker_process_.Terminate(exit_code, /*wait=*/true));
  worker_process_.Close();
  if (event_handler_) {
    event_handler_->OnProcessExited(exit_code);
  }
}

void WorkerProcessLauncherTest::ConnectClient() {
  channel_client_ = IPC::ChannelProxy::Create(
      client_channel_handle_.release(), IPC::Channel::MODE_CLIENT,
      &client_listener_, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  desktop_session_state_handler_.reset();
  channel_client_->GetRemoteAssociatedInterface(
      &desktop_session_state_handler_);

  worker_process_control_.reset();
  channel_server_->GetRemoteAssociatedInterface(&worker_process_control_);

  // Pretend that |kLaunchSuccessTimeoutSeconds| passed since launching
  // the worker process. This will make the backoff algorithm think that this
  // launch attempt was successful and it will not delay the next launch.
  launcher_->RecordSuccessfulLaunchForTest();
}

void WorkerProcessLauncherTest::DisconnectClient() {
  if (channel_client_) {
    channel_client_->Close();
    channel_client_.reset();
  }
  desktop_session_state_handler_.reset();
}

void WorkerProcessLauncherTest::DisconnectServer() {
  if (channel_server_) {
    channel_server_->Close();
    channel_server_.reset();
  }
  worker_process_control_.reset();
}

void WorkerProcessLauncherTest::SendFakeMessageToLauncher() {
  if (desktop_session_state_handler_) {
    desktop_session_state_handler_->DisconnectSession(
        ErrorCode::OK, /* error_details= */ {}, FROM_HERE);
  }
}

void WorkerProcessLauncherTest::CrashWorker() {
  launcher_->Crash(FROM_HERE);
}

void WorkerProcessLauncherTest::StartWorker() {
  launcher_ = std::make_unique<WorkerProcessLauncher>(
      std::move(launcher_delegate_), &server_listener_);

  launcher_->SetKillProcessTimeoutForTest(base::Milliseconds(100));
}

void WorkerProcessLauncherTest::StopWorker() {
  launcher_.reset();
  DisconnectClient();
  client_channel_handle_.reset();
  DisconnectServer();
  task_runner_ = nullptr;
}

void WorkerProcessLauncherTest::QuitMainMessageLoop() {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, loop_.QuitWhenIdleClosure());
}

void WorkerProcessLauncherTest::DoLaunchProcess() {
  EXPECT_TRUE(event_handler_);
  EXPECT_FALSE(worker_process_.IsValid());

  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();

  mojo::MessagePipe pipe;
  client_channel_handle_ = std::move(pipe.handle0);

  worker_process_ = base::SpawnMultiProcessTestChild(
      "WorkerProcessLauncherTestClient", command_line, {});
  ASSERT_TRUE(worker_process_.IsValid());

  channel_server_ = IPC::ChannelProxy::Create(
      pipe.handle1.release(), IPC::Channel::MODE_SERVER, this, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

TEST_F(WorkerProcessLauncherTest, Start) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::LaunchProcess));

  EXPECT_CALL(server_listener_, OnChannelConnected(_)).Times(0);
  EXPECT_CALL(server_listener_, OnPermanentError(_)).Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(0);

  StartWorker();
  StopWorker();
  Run();
}

// Starts and connects to the worker process. Expect OnChannelConnected to be
// called.
TEST_F(WorkerProcessLauncherTest, StartAndConnect) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(server_listener_, OnPermanentError(_)).Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(0);

  StartWorker();
  Run();
}

// Kills the worker process after the 1st connect and expects it to be
// restarted.
TEST_F(WorkerProcessLauncherTest, Restart) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));
  Expectation first_connect =
      EXPECT_CALL(server_listener_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(InvokeWithoutArgs(
              [this]() { TerminateWorker(kSuccessExitCode); }))
          .WillOnce(
              InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(server_listener_, OnPermanentError(_)).Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(1);

  StartWorker();
  Run();
}

// Drops the IPC channel to the worker process after the 1st connect and expects
// the worker process to be restarted.
TEST_F(WorkerProcessLauncherTest, DropIpcChannel) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  Expectation first_connect =
      EXPECT_CALL(server_listener_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(InvokeWithoutArgs(
              this, &WorkerProcessLauncherTest::DisconnectClient))
          .WillOnce(
              InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(server_listener_, OnPermanentError(_)).Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(1);

  StartWorker();
  Run();
}

// Returns a permanent error exit code and expects OnPermanentError() to be
// invoked.
TEST_F(WorkerProcessLauncherTest, PermanentError) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          [this] { TerminateWorker(kMinPermanentErrorExitCode); }));
  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(1);

  StartWorker();
  Run();
}

// Requests the worker to crash and expects it to honor the request.
TEST_F(WorkerProcessLauncherTest, Crash) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(2)
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::CrashWorker))
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(client_listener_, CrashProcess(_, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          [this]() { TerminateWorker(kInitializationFailed); }));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(1);

  StartWorker();
  Run();
}

// Requests the worker to crash and terminates the worker even if it does not
// comply.
TEST_F(WorkerProcessLauncherTest, CrashAnyway) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(2)
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::CrashWorker))
      .WillOnce(
          InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));

  // Ignore the crash request and try send another message to the launcher.
  EXPECT_CALL(client_listener_, CrashProcess(_, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &WorkerProcessLauncherTest::SendFakeMessageToLauncher));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped()).Times(1);

  StartWorker();
  Run();
}

MULTIPROCESS_TEST_MAIN(WorkerProcessLauncherTestClient) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);

  // Keep the process alive until the main process terminates it.
  base::RunLoop().Run();
  return 0;
}

}  // namespace remoting
