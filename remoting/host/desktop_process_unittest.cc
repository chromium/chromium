// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_process.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/fake_keyboard_layout_monitor.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/remote_open_url/fake_url_forwarder_configurator.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::ByMove;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

class MockDaemonListener : public IPC::Listener,
                           public mojom::DesktopSessionRequestHandler {
 public:
  MockDaemonListener() = default;

  MockDaemonListener(const MockDaemonListener&) = delete;
  MockDaemonListener& operator=(const MockDaemonListener&) = delete;

  ~MockDaemonListener() override = default;

  bool OnMessageReceived(const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  MOCK_METHOD(void,
              ConnectDesktopChannel,
              (mojo::ScopedMessagePipeHandle handle),
              (override));
  MOCK_METHOD(void, InjectSecureAttentionSequence, (), (override));
  MOCK_METHOD(void, CrashNetworkProcess, (), (override));
  MOCK_METHOD(void, OnChannelConnected, (int32_t), (override));
  MOCK_METHOD(void, OnChannelError, (), (override));

  void Disconnect();

 private:
  mojo::AssociatedReceiver<mojom::DesktopSessionRequestHandler>
      desktop_session_request_handler_{this};
};

class MockNetworkListener : public IPC::Listener {
 public:
  MockNetworkListener() = default;

  MockNetworkListener(const MockNetworkListener&) = delete;
  MockNetworkListener& operator=(const MockNetworkListener&) = delete;

  ~MockNetworkListener() override = default;

  bool OnMessageReceived(const IPC::Message& message) override;

  MOCK_METHOD(void, OnChannelConnected, (int32_t), (override));
  MOCK_METHOD(void, OnChannelError, (), (override));

  MOCK_METHOD0(OnDesktopEnvironmentCreated, void());
};

bool MockDaemonListener::OnMessageReceived(const IPC::Message& message) {
  ADD_FAILURE() << "Unexpected call to OnMessageReceived()";
  return false;
}

void MockDaemonListener::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  EXPECT_EQ(mojom::DesktopSessionRequestHandler::Name_, interface_name);
  mojo::PendingAssociatedReceiver<mojom::DesktopSessionRequestHandler>
      pending_receiver(std::move(handle));
  desktop_session_request_handler_.Bind(std::move(pending_receiver));
}

void MockDaemonListener::Disconnect() {
  desktop_session_request_handler_.reset();
}

bool MockNetworkListener::OnMessageReceived(const IPC::Message& message) {
  ADD_FAILURE() << "Unexpected call to OnMessageReceived()";
  return false;
}

}  // namespace

class DesktopProcessTest : public testing::Test {
 public:
  DesktopProcessTest();
  ~DesktopProcessTest() override;

  // Methods invoked when MockDaemonListener::ConnectDesktopChannel is called.
  void CreateNetworkChannel(mojo::ScopedMessagePipeHandle desktop_pipe);
  void StoreDesktopHandle(mojo::ScopedMessagePipeHandle desktop_pipe);

  // Creates a DesktopEnvironment with a fake webrtc::DesktopCapturer, to mock
  // DesktopEnvironmentFactory::Create().
  std::unique_ptr<DesktopEnvironment> CreateDesktopEnvironment();

  // Creates a fake InputInjector, to mock
  // DesktopEnvironment::CreateInputInjector().
  std::unique_ptr<InputInjector> CreateInputInjector();

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

  // Receives the DesktopSessionControl remote used to inject input and control
  // A/V capture in the test.
  void OnDesktopSessionAgentStarted(
      mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
          pending_remote);

 protected:
  // The daemon's end of the daemon-to-desktop channel.
  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  // Delegate that is passed to |daemon_channel_|.
  MockDaemonListener daemon_listener_;

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent_;
  mojo::AssociatedRemote<mojom::DesktopSessionControl> desktop_session_control_;
  mojo::AssociatedRemote<mojom::WorkerProcessControl> worker_process_control_;

  // Runs the daemon's end of the channel.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // The network's end of the network-to-desktop channel.
  std::unique_ptr<IPC::ChannelProxy> network_channel_;

  // Delegate that is passed to |network_channel_|.
  MockNetworkListener network_listener_;

  mojo::ScopedMessagePipeHandle desktop_pipe_handle_;
};

DesktopProcessTest::DesktopProcessTest() = default;

DesktopProcessTest::~DesktopProcessTest() = default;

void DesktopProcessTest::CreateNetworkChannel(
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  network_channel_ = IPC::ChannelProxy::Create(
      desktop_pipe.release(), IPC::Channel::MODE_CLIENT, &network_listener_,
      io_task_runner_.get(), base::SingleThreadTaskRunner::GetCurrentDefault());
}

void DesktopProcessTest::StoreDesktopHandle(
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  desktop_pipe_handle_ = std::move(desktop_pipe);
}

std::unique_ptr<DesktopEnvironment>
DesktopProcessTest::CreateDesktopEnvironment() {
  auto desktop_environment = std::make_unique<MockDesktopEnvironment>();
  EXPECT_CALL(*desktop_environment, CreateAudioCapturer()).Times(0);
  EXPECT_CALL(*desktop_environment, CreateInputInjector())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &DesktopProcessTest::CreateInputInjector));
  EXPECT_CALL(*desktop_environment, CreateActionExecutor()).Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateScreenControls()).Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, CreateVideoCapturer(_))
      .Times(AtMost(1))
      .WillOnce(
          Return(ByMove(std::make_unique<protocol::FakeDesktopCapturer>())));
  EXPECT_CALL(*desktop_environment, CreateMouseCursorMonitor())
      .Times(AtMost(1))
      .WillOnce(Return(ByMove(std::make_unique<FakeMouseCursorMonitor>())));
  EXPECT_CALL(*desktop_environment, CreateKeyboardLayoutMonitor(_))
      .Times(AtMost(1))
      .WillOnce(Return(ByMove(std::make_unique<FakeKeyboardLayoutMonitor>())));
  EXPECT_CALL(*desktop_environment, CreateUrlForwarderConfigurator())
      .Times(AtMost(1))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeUrlForwarderConfigurator>())));
  EXPECT_CALL(*desktop_environment, CreateFileOperations()).Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, GetCapabilities()).Times(AtMost(1));
  EXPECT_CALL(*desktop_environment, SetCapabilities(_)).Times(AtMost(1));

  // Notify the test that the desktop environment has been created.
  network_listener_.OnDesktopEnvironmentCreated();
  return desktop_environment;
}

std::unique_ptr<InputInjector> DesktopProcessTest::CreateInputInjector() {
  auto input_injector = std::make_unique<MockInputInjector>();
  EXPECT_CALL(*input_injector, Start(_));
  return input_injector;
}

void DesktopProcessTest::DisconnectChannels() {
  daemon_channel_.reset();
  desktop_pipe_handle_.reset();
  daemon_listener_.Disconnect();
  desktop_session_agent_.reset();
  desktop_session_control_.reset();
  worker_process_control_.reset();

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
  base::OnceClosure quit_ui_task_runner = base::BindOnce(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      task_environment_.GetMainThreadTaskRunner(), FROM_HERE,
      run_loop.QuitClosure());
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
      new AutoThreadTaskRunner(task_environment_.GetMainThreadTaskRunner(),
                               std::move(quit_ui_task_runner));

  io_task_runner_ = AutoThread::CreateWithType("IPC thread", ui_task_runner,
                                               base::MessagePumpType::IO);

  mojo::MessagePipe pipe;
  daemon_channel_ = IPC::ChannelProxy::Create(
      pipe.handle0.release(), IPC::Channel::MODE_SERVER, &daemon_listener_,
      io_task_runner_.get(), base::SingleThreadTaskRunner::GetCurrentDefault());

  std::unique_ptr<MockDesktopEnvironmentFactory> desktop_environment_factory(
      new MockDesktopEnvironmentFactory());
  EXPECT_CALL(*desktop_environment_factory, Create(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke(this, &DesktopProcessTest::CreateDesktopEnvironment));
  EXPECT_CALL(*desktop_environment_factory, SupportsAudioCapture())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  DesktopProcess desktop_process(ui_task_runner, io_task_runner_,
                                 io_task_runner_, std::move(pipe.handle1));
  EXPECT_TRUE(desktop_process.Start(std::move(desktop_environment_factory)));

  daemon_channel_->GetRemoteAssociatedInterface(&worker_process_control_);

  ui_task_runner = nullptr;
  run_loop.Run();
}

void DesktopProcessTest::RunDeathTest() {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, ConnectDesktopChannel(_))
      .WillOnce([&](mojo::ScopedMessagePipeHandle desktop_pipe) {
        StoreDesktopHandle(std::move(desktop_pipe));
        SendCrashRequest();
      });

  RunDesktopProcess();
}

void DesktopProcessTest::SendCrashRequest() {
  base::Location location = FROM_HERE;
  worker_process_control_->CrashProcess(
      location.function_name(), location.file_name(), location.line_number());
}

void DesktopProcessTest::SendStartSessionAgent() {
  desktop_session_agent_.reset();
  network_channel_->GetRemoteAssociatedInterface(&desktop_session_agent_);

  desktop_session_agent_->Start(
      "user@domain/rest-of-jid", ScreenResolution(),
      DesktopEnvironmentOptions(),
      base::BindOnce(&DesktopProcessTest::OnDesktopSessionAgentStarted,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

void DesktopProcessTest::OnDesktopSessionAgentStarted(
    mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
        pending_remote) {
  desktop_session_control_.reset();
  desktop_session_control_.Bind(std::move(pending_remote));
}

// Launches the desktop process and then disconnects immediately.
TEST_F(DesktopProcessTest, Basic) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, ConnectDesktopChannel(_))
      .WillOnce([&](mojo::ScopedMessagePipeHandle desktop_pipe) {
        StoreDesktopHandle(std::move(desktop_pipe));
        DisconnectChannels();
      });

  RunDesktopProcess();
}

// Launches the desktop process and waits until the IPC channel is established.
TEST_F(DesktopProcessTest, CreateNetworkChannel) {
  InSequence s;
  EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
  EXPECT_CALL(daemon_listener_, ConnectDesktopChannel(_))
      .WillOnce([&](mojo::ScopedMessagePipeHandle desktop_pipe) {
        CreateNetworkChannel(std::move(desktop_pipe));
      });
  EXPECT_CALL(network_listener_, OnChannelConnected(_))
      .WillOnce(
          InvokeWithoutArgs(this, &DesktopProcessTest::DisconnectChannels));

  RunDesktopProcess();
}

// Launches the desktop process, waits until the IPC channel is established,
// then starts the desktop session agent.
TEST_F(DesktopProcessTest, StartSessionAgent) {
  {
    InSequence s;
    EXPECT_CALL(daemon_listener_, OnChannelConnected(_));
    EXPECT_CALL(daemon_listener_, ConnectDesktopChannel(_))
        .WillOnce([&](mojo::ScopedMessagePipeHandle desktop_pipe) {
          CreateNetworkChannel(std::move(desktop_pipe));
        });
    EXPECT_CALL(network_listener_, OnChannelConnected(_))
        .WillOnce(InvokeWithoutArgs(
            this, &DesktopProcessTest::SendStartSessionAgent));
  }

  EXPECT_CALL(network_listener_, OnDesktopEnvironmentCreated())
      .WillOnce(
          InvokeWithoutArgs(this, &DesktopProcessTest::PostDisconnectChannels));

  RunDesktopProcess();
}

// Run the desktop process and ask it to crash.
TEST_F(DesktopProcessTest, DeathTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  EXPECT_DEATH(RunDeathTest(), "");
}

}  // namespace remoting
