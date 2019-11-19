// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_process.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

class MockDaemonListener : public IPC::Listener {
 public:
  MockDaemonListener() = default;
  ~MockDaemonListener() override = default;

  bool OnMessageReceived(const IPC::Message& message) override;

  MOCK_METHOD1(OnDesktopAttached, void(const IPC::ChannelHandle&));
  MOCK_METHOD1(OnChannelConnected, void(int32_t));
  MOCK_METHOD0(OnChannelError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDaemonListener);
};

class MockNetworkListener : public IPC::Listener {
 public:
  MockNetworkListener() = default;
  ~MockNetworkListener() override = default;

  bool OnMessageReceived(const IPC::Message& message) override;

  MOCK_METHOD1(OnChannelConnected, void(int32_t));
  MOCK_METHOD0(OnChannelError, void());

  MOCK_METHOD0(OnDesktopEnvironmentCreated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkListener);
};

bool MockDaemonListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockDaemonListener, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_DesktopAttached,
                        OnDesktopAttached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);
  return handled;
}

bool MockNetworkListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  // TODO(alexeypa): handle received messages here.

  EXPECT_TRUE(handled);
  return handled;
}

}  // namespace

class DesktopProcessTest : public testing::Test {
 public:
  DesktopProcessTest();
  ~DesktopProcessTest() override;

  // MockDaemonListener mocks
  void ConnectNetworkChannel(const IPC::ChannelHandle& desktop_process);
  void OnDesktopAttached(const IPC::ChannelHandle& desktop_process);

  // Creates a DesktopEnvironment with a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  DesktopEnvironment* CreateDesktopEnvironment();

  // Creates a dummy InputInjector, to mock
  // DesktopEnvironment::CreateInputInjector().
  InputInjector* CreateInputInjector();

  // Creates a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironment::CreateVideoCapturer().
  webrtc::DesktopCapturer* CreateVideoCapturer();

  // Creates a fake webrtc::MouseCursorMonitor, to mock
  // DesktopEnvironment::CreateMouseCursorMonitor().
  webrtc::MouseCursorMonitor* CreateMouseCursorMonitor();

  // Disconnects the daemon-to-desktop channel causing the desktop process to
  // exit.
  void DisconnectChannels();

  // Posts DisconnectChannels() to |message_loop_|.
  void PostDisconnectChannels();

  // Runs the desktop process code in a separate thread.
  void RunDesktopProcess();

  // Creates the desktop process and sends a crash request to it.
  void RunDeathTest();

  // Sends a crash request to the desktop process.
  void SendCrashRequest();

  // Requests the desktop process to start the desktop session agent.
  void SendStartSessionAgent();

 protected:
  // The daemon's end of the daemon-to-desktop channel.
  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  // Delegate that is passed to |daemon_channel_|.
  MockDaemonListener daemon_listener_;

  // Runs the daemon's end of the channel.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // The network's end of the network-to-desktop channel.
  std::unique_ptr<IPC::ChannelProxy> network_channel_;

  // Delegate that is passed to |network_channel_|.
  MockNetworkListener network_listener_;

  mojo::ScopedMessagePipeHandle desktop_process_channel_;
};

DesktopProcessTest::DesktopProcessTest() = default;

DesktopProcessTest::~DesktopProcessTest() = default;

void DesktopProcessTest::ConnectNetworkChannel(
    const IPC::ChannelHandle& channel_handle) {
  network_channel_ = IPC::ChannelProxy::Create(
      channel_handle, IPC::Channel::MODE_CLIENT, &network_listener_,
      io_task_runner_.get(), base::ThreadTaskRunnerHandle::Get());
}

void DesktopProcessTest::OnDesktopAttached(
    const IPC::ChannelHandle& desktop_process) {
  desktop_process_channel_.reset(desktop_process.mojo_handle);
}

DesktopEnvironment* DesktopProcessTest::CreateDesktopEnvironment() {
  MockDesktopEnvironment* desktop_environment = new MockDesktopEnvironment();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturerPtr())
      .Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjectorPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateScreenControlsPtr())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturerPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateVideoCapturer));
  EXPECT_CALL(*desktop_environment, CreateMouseCursorMonitorPtr())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateMouseCursorMonitor));
  EXPECT_CALL(*desktop_environment, GetCapabilities())
      .Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_))
      .Times(AtMost(1));

  // Notify the test that the desktop environment has been created.
  network_listener_.OnDesktopEnvironmentCreated();
  return desktop_environment;
}

InputInjector* DesktopProcessTest::CreateInputInjector() {
  MockInputInjector* input_injector = new MockInputInjector();
  EXPECT_CALL(*input_injector, StartPtr(_));
  return input_injector;
}

webrtc::DesktopCapturer* DesktopProcessTest::CreateVideoCapturer() {
  return new protocol::FakeDesktopCapturer();
}

webrtc::MouseCursorMonitor* DesktopProcessTest::CreateMouseCursorMonitor() {
  return new FakeMouseCursorMonitor();
}

void DesktopProcessTest::DisconnectChannels() {
  daemon_channel_.reset();
  network_channel_.reset();
  io_task_runner_ = nullptr;
}

void DesktopProcessTest::PostDisconnectChannels() {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DesktopProcessTest::DisconnectChannels,
                                base::Unretained(this)));
}

void DesktopProcessTest::RunDesktopProcess() {
  base::RunLoop run_loop;
  base::Closure quit_ui_task_runner =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 task_environment_.GetMainThreadTaskRunner(), FROM_HERE,
                 run_loop.QuitClosure());
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), quit_ui_task_runner);

  io_task_runner_ = AutoThread::CreateWithType("IPC thread", ui_task_runner,
                                               base::MessagePumpType::IO);

  mojo::MessagePipe pipe;
  daemon_channel_ = IPC::ChannelProxy::Create(
      pipe.handle0.release(), IPC::Channel::MODE_SERVER, &daemon_listener_,
      io_task_runner_.get(), base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory(
      new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory, CreatePtr())
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this,
                             &DesktopProcessTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  DesktopProcess desktop_process(ui_task_runner, io_task_runner_,
                                 io_task_runner_, std::move(pipe.handle1));
  EXPECT_TRUE(desktop_process.Start(std::move(desktop_environment_factory)));

  ui_task_runner = nullptr;
  run_loop.Run();
}

void DesktopProcessTest::RunDeathTest() {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(DoAll(
          Invoke(this, &DesktopProcessTest::OnDesktopAttached),
          InvokeWithoutArgs(this, &DesktopProcessTest::SendCrashRequest)));

  RunDesktopProcess();
}

void DesktopProcessTest::SendCrashRequest() {
  base::Location location = FROM_HERE;
  daemon_channel_->Send(new ChromotingDaemonMsg_Crash(
      location.function_name(), location.file_name(), location.line_number()));
}

void DesktopProcessTest::SendStartSessionAgent() {
  network_channel_->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "user@domain/rest-of-jid", ScreenResolution(),
      DesktopEnvironmentOptions()));
}

// Launches the desktop process and waits when it connects back.
TEST_F(DesktopProcessTest, Basic) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(DoAll(
          Invoke(this, &DesktopProcessTest::OnDesktopAttached),
          InvokeWithoutArgs(this, &DesktopProcessTest::DisconnectChannels)));

  RunDesktopProcess();
}

// Launches the desktop process and waits when it connects back.
TEST_F(DesktopProcessTest, ConnectNetworkChannel) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
      .WillOnce(Invoke(this, &DesktopProcessTest::ConnectNetworkChannel));
  EXPECT_CALL(network_listener_, OnChannelConnected(_))
      .WillOnce(InvokeWithoutArgs(
          this, &DesktopProcessTest::DisconnectChannels));

  RunDesktopProcess();
}

// Launches the desktop process, waits when it connects back and starts
// the desktop session agent.
TEST_F(DesktopProcessTest, StartSessionAgent) {
  {
    InSequence s;
    EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
    EXPECT_CALL(daemon_listener_, OnDesktopAttached(_))
        .WillOnce(Invoke(this, &DesktopProcessTest::ConnectNetworkChannel));
    EXPECT_CALL(network_listener_, OnChannelConnected(_))
        .WillOnce(InvokeWithoutArgs(
            this, &DesktopProcessTest::SendStartSessionAgent));
  }

  EXPECT_CALL(network_listener_, OnDesktopEnvironmentCreated())
      .WillOnce(InvokeWithoutArgs(
          this, &DesktopProcessTest::PostDisconnectChannels));

  RunDesktopProcess();
}

// Run the desktop process and ask it to crash.
TEST_F(DesktopProcessTest, DeathTest) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";

  EXPECT_DEATH(RunDeathTest(), "");
}

}  // namespace remoting
