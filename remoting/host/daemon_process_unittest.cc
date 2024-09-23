// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/desktop_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;

namespace remoting {

namespace {

// Provides a public constructor allowing the test to create instances of
// DesktopSession directly.
class FakeDesktopSession : public DesktopSession {
 public:
  FakeDesktopSession(DaemonProcess* daemon_process, int id);

  FakeDesktopSession(const FakeDesktopSession&) = delete;
  FakeDesktopSession& operator=(const FakeDesktopSession&) = delete;

  ~FakeDesktopSession() override;

  void SetScreenResolution(const ScreenResolution& resolution) override {}
};

class MockDaemonProcess : public DaemonProcess {
 public:
  MockDaemonProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                    base::OnceClosure stopped_callback);

  MockDaemonProcess(const MockDaemonProcess&) = delete;
  MockDaemonProcess& operator=(const MockDaemonProcess&) = delete;

  ~MockDaemonProcess() override;

  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) override;

  MOCK_METHOD(bool,
              OnDesktopSessionAgentAttached,
              (int, int, mojo::ScopedMessagePipeHandle),
              (override));

  MOCK_METHOD(DesktopSession*, DoCreateDesktopSessionPtr, (int));
  MOCK_METHOD(void, DoCrashNetworkProcess, (const base::Location&), (override));
  MOCK_METHOD(void, LaunchNetworkProcess, (), (override));
  MOCK_METHOD(void,
              SendHostConfigToNetworkProcess,
              (const std::string&),
              (override));
  MOCK_METHOD(void, SendTerminalDisconnected, (int terminal_id), (override));
  MOCK_METHOD(void, StartChromotingHostServices, (), (override));
};

FakeDesktopSession::FakeDesktopSession(DaemonProcess* daemon_process, int id)
    : DesktopSession(daemon_process, id) {}

FakeDesktopSession::~FakeDesktopSession() = default;

MockDaemonProcess::MockDaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback)
    : DaemonProcess(caller_task_runner,
                    io_task_runner,
                    std::move(stopped_callback)) {}

MockDaemonProcess::~MockDaemonProcess() = default;

std::unique_ptr<DesktopSession> MockDaemonProcess::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  return base::WrapUnique(DoCreateDesktopSessionPtr(terminal_id));
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  std::unique_ptr<MockDaemonProcess> daemon_process_;
  int terminal_id_ = 0;
  base::RunLoop run_loop_;
};

DaemonProcessTest::DaemonProcessTest() = default;

DaemonProcessTest::~DaemonProcessTest() = default;

void DaemonProcessTest::SetUp() {
  scoped_refptr<AutoThreadTaskRunner> task_runner = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(),
      base::BindOnce(&DaemonProcessTest::QuitMessageLoop,
                     base::Unretained(this)));
  daemon_process_ = std::make_unique<MockDaemonProcess>(
      task_runner, task_runner,
      base::BindOnce(&DaemonProcessTest::DeleteDaemonProcess,
                     base::Unretained(this)));

  // Set up daemon process mocks.
  EXPECT_CALL(*daemon_process_, DoCreateDesktopSessionPtr(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::DoCreateDesktopSession));
  EXPECT_CALL(*daemon_process_, DoCrashNetworkProcess(_)).Times(AnyNumber());
  EXPECT_CALL(*daemon_process_, LaunchNetworkProcess())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, StartChromotingHostServices())
      .Times(AnyNumber());
}

void DaemonProcessTest::TearDown() {
  daemon_process_->Stop();
  run_loop_.Run();
}

DesktopSession* DaemonProcessTest::DoCreateDesktopSession(int terminal_id) {
  return new FakeDesktopSession(daemon_process_.get(), terminal_id);
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
      FROM_HERE, run_loop_.QuitWhenIdleClosure());
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
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));
  EXPECT_CALL(*daemon_process_, SendTerminalDisconnected(_));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  daemon_process_->CreateDesktopSession(id, resolution, false);
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
}

TEST_F(DaemonProcessTest, CallCloseDesktopSession) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));
  EXPECT_CALL(*daemon_process_, SendTerminalDisconnected(_));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  daemon_process_->CreateDesktopSession(id, resolution, false);
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
}

// Sends two CloseDesktopSession messages and expects the second one to be
// ignored.
TEST_F(DaemonProcessTest, DoubleDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));
  EXPECT_CALL(*daemon_process_, SendTerminalDisconnected(_));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  daemon_process_->CreateDesktopSession(id, resolution, false);
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
}

// Tries to close an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidDisconnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));
  EXPECT_CALL(*daemon_process_, DoCrashNetworkProcess(_))
      .WillOnce(
          InvokeWithoutArgs(this, &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));

  StartDaemonProcess();

  int id = terminal_id_++;

  daemon_process_->CloseDesktopSession(id);
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

// Tries to open an invalid terminal ID and expects the network process to be
// restarted.
TEST_F(DaemonProcessTest, InvalidConnectTerminal) {
  InSequence s;
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));
  EXPECT_CALL(*daemon_process_, DoCrashNetworkProcess(_))
      .WillOnce(
          InvokeWithoutArgs(this, &DaemonProcessTest::LaunchNetworkProcess));
  EXPECT_CALL(*daemon_process_, SendHostConfigToNetworkProcess(_));

  StartDaemonProcess();

  int id = terminal_id_++;
  ScreenResolution resolution;

  daemon_process_->CreateDesktopSession(id, resolution, false);
  EXPECT_EQ(1u, desktop_sessions().size());
  EXPECT_EQ(id, desktop_sessions().front()->id());

  daemon_process_->CreateDesktopSession(id, resolution, false);
  EXPECT_TRUE(desktop_sessions().empty());
  EXPECT_EQ(0, terminal_id_);
}

}  // namespace remoting
