// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/worker_process_launcher.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedHandle;
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
  ~MockProcessLauncherDelegate() override {}

  // WorkerProcessLauncher::Delegate interface.
  MOCK_METHOD1(LaunchProcess, void(WorkerProcessLauncher*));
  MOCK_METHOD1(Send, void(IPC::Message*));
  MOCK_METHOD0(CloseChannel, void());
  MOCK_METHOD0(KillProcess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessLauncherDelegate);
};

class MockIpcDelegate : public WorkerProcessIpcDelegate {
 public:
  MockIpcDelegate() {}
  ~MockIpcDelegate() override {}

  // WorkerProcessIpcDelegate interface.
  MOCK_METHOD1(OnChannelConnected, void(int32_t));
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message&));
  MOCK_METHOD1(OnPermanentError, void(int));
  MOCK_METHOD0(OnWorkerProcessStopped, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIpcDelegate);
};

class MockWorkerListener : public IPC::Listener {
 public:
  MockWorkerListener() {}
  ~MockWorkerListener() override {}

  MOCK_METHOD3(OnCrash, void(const std::string&, const std::string&, int));

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWorkerListener);
};

bool MockWorkerListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockWorkerListener, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonMsg_Crash, OnCrash)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);

  return handled;
}

}  // namespace

class WorkerProcessLauncherTest
    : public testing::Test,
      public IPC::Listener {
 public:
  WorkerProcessLauncherTest();
  ~WorkerProcessLauncherTest() override;

  void SetUp() override;
  void TearDown() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // WorkerProcessLauncher::Delegate mocks
  void LaunchProcess(
      WorkerProcessLauncher* event_handler);
  void LaunchProcessAndConnect(
      WorkerProcessLauncher* event_handler);
  void FailLaunchAndStopWorker(
      WorkerProcessLauncher* event_handler);
  void KillProcess();

  void TerminateWorker(DWORD exit_code);

  // Connects the client end of the channel (the worker process's end).
  void ConnectClient();

  // Disconnects the client end of the channel.
  void DisconnectClient();

  // Disconnects the server end of the channel (the launcher's end).
  void DisconnectServer();

  // Sends a message to the worker process.
  void SendToProcess(IPC::Message* message);

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

  WorkerProcessLauncher* event_handler_;

  // The worker process launcher.
  std::unique_ptr<WorkerProcessLauncher> launcher_;

  // An event that is used to emulate the worker process's handle.
  ScopedHandle worker_process_;
};

WorkerProcessLauncherTest::WorkerProcessLauncherTest()
    : event_handler_(nullptr) {
}

WorkerProcessLauncherTest::~WorkerProcessLauncherTest() {
}

void WorkerProcessLauncherTest::SetUp() {
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(),
      base::BindOnce(&WorkerProcessLauncherTest::QuitMainMessageLoop,
                     base::Unretained(this)));

  // Set up process launcher delegate
  launcher_delegate_.reset(new MockProcessLauncherDelegate());
  EXPECT_CALL(*launcher_delegate_, Send(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::SendToProcess));
  EXPECT_CALL(*launcher_delegate_, CloseChannel())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &WorkerProcessLauncherTest::DisconnectServer));
  EXPECT_CALL(*launcher_delegate_, KillProcess())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::KillProcess));

  // Set up IPC delegate.
  EXPECT_CALL(server_listener_, OnMessageReceived(_))
      .Times(0);
}

void WorkerProcessLauncherTest::TearDown() {
}

bool WorkerProcessLauncherTest::OnMessageReceived(const IPC::Message& message) {
  return event_handler_->OnMessageReceived(message);
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

void WorkerProcessLauncherTest::KillProcess() {
  event_handler_ = nullptr;

  DisconnectClient();
  if (worker_process_.IsValid()) {
    TerminateProcess(worker_process_.Get(), CONTROL_C_EXIT);
    worker_process_.Close();
  }
}

void WorkerProcessLauncherTest::TerminateWorker(DWORD exit_code) {
  if (worker_process_.IsValid())
    TerminateProcess(worker_process_.Get(), exit_code);
}

void WorkerProcessLauncherTest::ConnectClient() {
  channel_client_ = IPC::ChannelProxy::Create(
      client_channel_handle_.release(), IPC::Channel::MODE_CLIENT,
      &client_listener_, task_runner_, base::ThreadTaskRunnerHandle::Get());

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
}

void WorkerProcessLauncherTest::DisconnectServer() {
  if (channel_server_) {
    channel_server_->Close();
    channel_server_.reset();
  }
}

void WorkerProcessLauncherTest::SendToProcess(IPC::Message* message) {
  if (channel_server_) {
    channel_server_->Send(message);
    return;
  }

  delete message;
}

void WorkerProcessLauncherTest::SendFakeMessageToLauncher() {
  if (channel_client_) {
    channel_client_->Send(
        new ChromotingDesktopNetworkMsg_DisconnectSession(protocol::OK));
  }
}

void WorkerProcessLauncherTest::CrashWorker() {
  launcher_->Crash(FROM_HERE);
}

void WorkerProcessLauncherTest::StartWorker() {
  launcher_.reset(new WorkerProcessLauncher(std::move(launcher_delegate_),
                                            &server_listener_));

  launcher_->SetKillProcessTimeoutForTest(
      base::TimeDelta::FromMilliseconds(100));
}

void WorkerProcessLauncherTest::StopWorker() {
  launcher_.reset();
  DisconnectClient();
  client_channel_handle_.reset();
  channel_server_.reset();
  task_runner_ = nullptr;
}

void WorkerProcessLauncherTest::QuitMainMessageLoop() {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
}

void WorkerProcessLauncherTest::DoLaunchProcess() {
  EXPECT_TRUE(event_handler_);
  EXPECT_FALSE(worker_process_.IsValid());

  WCHAR notepad[MAX_PATH + 1];
  ASSERT_GT(ExpandEnvironmentStrings(
      L"\045SystemRoot\045\\system32\\notepad.exe", notepad, MAX_PATH), 0u);

  STARTUPINFOW startup_info = { 0 };
  startup_info.cb = sizeof(startup_info);

  PROCESS_INFORMATION temp_process_info = {};
  ASSERT_TRUE(CreateProcess(nullptr,
                            notepad,
                            nullptr,   // default process attibutes
                            nullptr,   // default thread attibutes
                            FALSE,  // do not inherit handles
                            CREATE_SUSPENDED,
                            nullptr,   // no environment
                            nullptr,   // default current directory
                            &startup_info,
                            &temp_process_info));
  base::win::ScopedProcessInformation process_information(temp_process_info);
  worker_process_.Set(process_information.TakeProcessHandle());
  ASSERT_TRUE(worker_process_.IsValid());

  mojo::MessagePipe pipe;
  client_channel_handle_ = std::move(pipe.handle0);

  // Wrap the pipe into an IPC channel.
  channel_server_ = IPC::ChannelProxy::Create(
      pipe.handle1.release(), IPC::Channel::MODE_SERVER, this, task_runner_,
      base::ThreadTaskRunnerHandle::Get());

  HANDLE temp_handle;
  ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), worker_process_.Get(),
                              GetCurrentProcess(), &temp_handle, 0, FALSE,
                              DUPLICATE_SAME_ACCESS));
  event_handler_->OnProcessLaunched(ScopedHandle(temp_handle));
}

TEST_F(WorkerProcessLauncherTest, Start) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(Invoke(this, &WorkerProcessLauncherTest::LaunchProcess));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(0);
  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(0);

  StartWorker();
  StopWorker();
  base::RunLoop().Run();
}

// Starts and connects to the worker process. Expect OnChannelConnected to be
// called.
TEST_F(WorkerProcessLauncherTest, StartAndConnect) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(0);

  StartWorker();
  base::RunLoop().Run();
}

// Kills the worker process after the 1st connect and expects it to be
// restarted.
TEST_F(WorkerProcessLauncherTest, Restart) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));
  Expectation first_connect =
      EXPECT_CALL(server_listener_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(
              InvokeWithoutArgs([=]() { TerminateWorker(CONTROL_C_EXIT); }))
          .WillOnce(
              InvokeWithoutArgs(this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(1);

  StartWorker();
  base::RunLoop().Run();
}

// Drops the IPC channel to the worker process after the 1st connect and expects
// the worker process to be restarted.
TEST_F(WorkerProcessLauncherTest, DropIpcChannel) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  Expectation first_connect =
      EXPECT_CALL(server_listener_, OnChannelConnected(_))
          .Times(2)
          .WillOnce(InvokeWithoutArgs(
              this, &WorkerProcessLauncherTest::DisconnectClient))
          .WillOnce(InvokeWithoutArgs(
              this, &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(0);
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(1);

  StartWorker();
  base::RunLoop().Run();
}

// Returns a permanent error exit code and expects OnPermanentError() to be
// invoked.
TEST_F(WorkerProcessLauncherTest, PermanentError) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(1)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          [=] { TerminateWorker(kMinPermanentErrorExitCode); }));
  EXPECT_CALL(server_listener_, OnPermanentError(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(1);

  StartWorker();
  base::RunLoop().Run();
}

// Requests the worker to crash and expects it to honor the request.
TEST_F(WorkerProcessLauncherTest, Crash) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(2)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::CrashWorker))
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));

  EXPECT_CALL(client_listener_, OnCrash(_, _, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs([=]() { TerminateWorker(EXCEPTION_BREAKPOINT); }));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(1);

  StartWorker();
  base::RunLoop().Run();
}

// Requests the worker to crash and terminates the worker even if it does not
// comply.
TEST_F(WorkerProcessLauncherTest, CrashAnyway) {
  EXPECT_CALL(*launcher_delegate_, LaunchProcess(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          this, &WorkerProcessLauncherTest::LaunchProcessAndConnect));

  EXPECT_CALL(server_listener_, OnChannelConnected(_))
      .Times(2)
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::CrashWorker))
      .WillOnce(InvokeWithoutArgs(this,
                                  &WorkerProcessLauncherTest::StopWorker));

  // Ignore the crash request and try send another message to the launcher.
  EXPECT_CALL(client_listener_, OnCrash(_, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(
          this, &WorkerProcessLauncherTest::SendFakeMessageToLauncher));
  EXPECT_CALL(server_listener_, OnWorkerProcessStopped())
      .Times(1);

  StartWorker();
  base::RunLoop().Run();
}

}  // namespace remoting
