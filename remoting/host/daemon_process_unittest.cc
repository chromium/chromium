// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

namespace remoting {

namespace {

enum Messages {
  kMessageCrash = ChromotingDaemonMsg_Crash::ID,
  kMessageConfiguration = ChromotingDaemonNetworkMsg_Configuration::ID,
  kMessageConnectTerminal = ChromotingNetworkHostMsg_ConnectTerminal::ID,
  kMessageDisconnectTerminal = ChromotingNetworkHostMsg_DisconnectTerminal::ID,
  kMessageTerminalDisconnected =
      ChromotingDaemonNetworkMsg_TerminalDisconnected::ID,
  kMessageReportProcessStats = ChromotingAnyToNetworkMsg_ReportProcessStats::ID,
};

// Provides a public constructor allowing the test to create instances of
// DesktopSession directly.
class FakeDesktopSession : public DesktopSession {
 public:
  FakeDesktopSession(DaemonProcess* daemon_process, int id);
  ~FakeDesktopSession() override;

  void SetScreenResolution(const ScreenResolution& resolution) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDesktopSession);
};

class MockDaemonProcess : public DaemonProcess {
 public:
  MockDaemonProcess(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);
  ~MockDaemonProcess() override;

  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) override;

  bool OnMessageReceived(const IPC::Message& message) override;
  void SendToNetwork(IPC::Message* message) override;

  MOCK_METHOD1(Received, void(const IPC::Message&));
  MOCK_METHOD1(Sent, void(const IPC::Message&));

  MOCK_METHOD3(OnDesktopSessionAgentAttached,
               bool(int, int, const IPC::ChannelHandle&));

  MOCK_METHOD1(DoCreateDesktopSessionPtr, DesktopSession*(int));
  MOCK_METHOD1(DoCrashNetworkProcess, void(const base::Location&));
  MOCK_METHOD0(LaunchNetworkProcess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemonProcess);
};

FakeDesktopSession::FakeDesktopSession(DaemonProcess* daemon_process, int id)
    : DesktopSession(daemon_process, id) {
}

FakeDesktopSession::~FakeDesktopSession() = default;

MockDaemonProcess::MockDaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(caller_task_runner, io_task_runner, stopped_callback) {
}

MockDaemonProcess::~MockDaemonProcess() = default;

std::unique_ptr<DesktopSession> MockDaemonProcess::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  return base::WrapUnique(DoCreateDesktopSessionPtr(terminal_id));
}

bool MockDaemonProcess::OnMessageReceived(const IPC::Message& message) {
  // Notify the mock method.
  Received(message);

  // Call the actual handler.
  return DaemonProcess::OnMessageReceived(message);
}

void MockDaemonProcess::SendToNetwork(IPC::Message* message) {
  // Notify the mock method.
  Sent(*message);
  delete message;
}

}  // namespace

class DaemonProcessTest : public testing::Test {
 public:
  DaemonProcessTest();
  ~DaemonProcessTest() override;

  void SetUp() override;
  void TearDown() override;

  // DaemonProcess mocks
  DesktopSession* DoCreateDesktopSession(int terminal_id);
  void DoCrashNetworkProcess(const base::Location& location);
  void LaunchNetworkProcess();

  // Deletes |daemon_process_|.
  void DeleteDaemonProcess();

  // Quits |message_loop_|.
  void QuitMessageLoop();

  void StartDaemonProcess();

  const DaemonProcess::DesktopSessionList& desktop_sessions() const {
    return daemon_process_->desktop_sessions();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<MockDaemonProcess> daemon_process_;
  int terminal_id_;
};

DaemonProcessTest::DaemonProcessTest() : terminal_id_(0) {
}

DaemonProcessTest::~DaemonProcessTest() = default;

void DaemonProcessTest::SetUp() {
  scoped_refptr<AutoThreadTaskRunner> task_runner = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(),
      base::Bind(&DaemonProcessTest::QuitMessageLoop, base::Unretained(this)));
  daemon_process_.reset(
      new MockDaemonProcess(task_runner, task_runner,
                            base::Bind(&DaemonProcessTest::DeleteDaemonProcess,
                                       base::Unretained(this))));

  // Set up daemon process mocks.
  EXPECT_CALL(*daemon_process_, DoCreateDesktopSessionPtr(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::DoCreateDesktopSession));
  EXPECT_CALL(*daemon_process_, DoCrashNetworkProcess(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::DoCrashNetworkProcess));
  EXPECT_CALL(*daemon_process_, LaunchNetworkProcess())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::LaunchNetworkProcess));
}

void DaemonProcessTest::TearDown() {
  daemon_process_->Stop();
  base::RunLoop().Run();
}

DesktopSession* DaemonProcessTest::DoCreateDesktopSession(int terminal_id) {
  return new FakeDesktopSession(daemon_process_.get(), terminal_id);
}

void DaemonProcessTest::DoCrashNetworkProcess(const base::Location& location) {
  daemon_process_->SendToNetwork(
      new ChromotingDaemonMsg_Crash(location.function_name(),
                                    location.file_name(),
                                    location.line_number()));
}

void DaemonProcessTest::LaunchNetworkProcess() {
  terminal_id_ = 0;
  daemon_process_->OnChannelConnected(0);
}

void DaemonProcessTest::DeleteDaemonProcess() {
  daemon_process_.reset();
}

void DaemonProcessTest::QuitMessageLoop() {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
}

void DaemonProcessTest::StartDaemonProcess() {
  // DaemonProcess::Initialize() sets up the config watcher that this test does
  // not support. Launch the process directly.
  daemon_process_->LaunchNetworkProcess();
}

MATCHER_P(Message, type, "") {
  return arg.type() == static_cast<uint32_t>(type);
}

TEST_F(DaemonProcessTest, OpenClose) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
}

TEST_F(DaemonProcessTest, CallCloseDesktopSession) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
}

// Sends two CloseDesktopSession messages and expects the second one to be
// ignored.
TEST_F(DaemonProcessTest, DoubleDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageTerminalDisconnected)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
}

// Tries to close an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageDisconnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageCrash)))
      .WillOnce(InvokeWithoutArgs(this,
                                  &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));

  StartDaemonProcess();

  int id = terminal_id_++;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_DisconnectTerminal(id)));
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

// Tries to open an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidConnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Received(Message(kMessageConnectTerminal)));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageCrash)))
      .WillOnce(InvokeWithoutArgs(this,
                                  &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageConfiguration)));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  EXPECT_TRUE(daemon_process_->OnMessageReceived(
      ChromotingNetworkHostMsg_ConnectTerminal(id, resolution, false)));
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

TEST_F(DaemonProcessTest, StartProcessStatsReport) {
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageReportProcessStats)));
  daemon_process_->OnMessageReceived(
      ChromotingNetworkToAnyMsg_StartProcessStatsReport(
          base::TimeDelta::FromMilliseconds(1)));
  base::RunLoop run_loop;
  ON_CALL(*daemon_process_, Sent(Message(kMessageReportProcessStats)))
      .WillByDefault(testing::Invoke(
          [&run_loop](const IPC::Message& message) {
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(DaemonProcessTest, StartProcessStatsReportWithDifferentDelta) {
  EXPECT_CALL(*daemon_process_, Sent(Message(kMessageReportProcessStats)))
      .Times(AnyNumber());
  int received = 0;
  daemon_process_->OnMessageReceived(
      ChromotingNetworkToAnyMsg_StartProcessStatsReport(
          base::TimeDelta::FromHours(1)));
  daemon_process_->OnMessageReceived(
      ChromotingNetworkToAnyMsg_StartProcessStatsReport(
          base::TimeDelta::FromMilliseconds(1)));
  base::RunLoop run_loop;
  ON_CALL(*daemon_process_, Sent(Message(kMessageReportProcessStats)))
      .WillByDefault(testing::Invoke(
          [&run_loop, &received](const IPC::Message& message) {
            received++;
            if (received == 5) {
              run_loop.Quit();
            }
          }));
  run_loop.Run();
}

TEST_F(DaemonProcessTest, StopProcessStatsReportWhenTheWorkerProcessDied) {
  daemon_process_->OnMessageReceived(
      ChromotingNetworkToAnyMsg_StartProcessStatsReport(
          base::TimeDelta::FromMilliseconds(1)));
  base::RunLoop run_loop;
  ON_CALL(*daemon_process_, Sent(Message(kMessageReportProcessStats)))
      .WillByDefault(testing::Invoke(
          [](const IPC::Message& message) {
            ASSERT_TRUE(false);
          }));
  static_cast<WorkerProcessIpcDelegate*>(daemon_process_.get())
      ->OnWorkerProcessStopped();
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(10));
  run_loop.Run();
}

}  // namespace remoting
