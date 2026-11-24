// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/errors.h"
#include "remoting/base/source_location.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::NiceMock;
using testing::Return;

class MockDesktopSessionEventHandler
    : public mojom::DesktopSessionEventHandler {
 public:
  MOCK_METHOD(void,
              OnClipboardEvent,
              (const protocol::ClipboardEvent& event),
              (override));
  MOCK_METHOD(void,
              OnUrlForwarderStateChange,
              (mojom::UrlForwarderState state),
              (override));
  MOCK_METHOD(void,
              OnAudioPacket,
              (std::unique_ptr<AudioPacket> audio_packet),
              (override));
  MOCK_METHOD(void,
              OnDesktopDisplayChanged,
              (const protocol::VideoLayout& layout),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorChanged,
              (const webrtc::MouseCursor& mouse_cursor),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorFractionalPositionChanged,
              (const protocol::FractionalCoordinate& position),
              (override));
  MOCK_METHOD(void,
              OnKeyboardLayoutChanged,
              (const protocol::KeyboardLayout& layout),
              (override));
  MOCK_METHOD(void,
              OnLocalMouseMoveDetected,
              (const webrtc::DesktopVector& new_position),
              (override));
  MOCK_METHOD(void,
              OnLocalKeyboardInputDetected,
              (int32_t usb_keycode),
              (override));
  MOCK_METHOD(void,
              OnSecurityKeyConnection,
              (mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver),
              (override));
  MOCK_METHOD(void,
              OnMicrophoneControl,
              (const protocol::MicrophoneControl& control),
              (override));
};

class NullUrlForwarderDesktopEnvironment : public FakeDesktopEnvironment {
 public:
  using FakeDesktopEnvironment::FakeDesktopEnvironment;

  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator()
      override {
    return nullptr;
  }
};

class NullActionExecutorDesktopEnvironment : public FakeDesktopEnvironment {
 public:
  using FakeDesktopEnvironment::FakeDesktopEnvironment;

  std::unique_ptr<ActionExecutor> CreateActionExecutor() override {
    return nullptr;
  }

  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator()
      override {
    return nullptr;
  }
};

class MockDesktopSessionStateHandler
    : public mojom::DesktopSessionStateHandler {
 public:
  MOCK_METHOD(void,
              DisconnectSession,
              (ErrorCode error_code,
               const std::string& error_details,
               const SourceLocation& error_location),
              (override));
};

class FakeDelegate : public DesktopSessionAgent::Delegate {
 public:
  explicit FakeDelegate(scoped_refptr<base::SingleThreadTaskRunner> runner)
      : factory_(std::make_unique<FakeDesktopEnvironmentFactory>(runner)) {}
  explicit FakeDelegate(std::unique_ptr<DesktopEnvironmentFactory> factory)
      : factory_(std::move(factory)) {}
  ~FakeDelegate() override = default;

  DesktopEnvironmentFactory& desktop_environment_factory() override {
    return *factory_;
  }

  void OnDesktopEnvironmentCreated(bool success) override {
    desktop_environment_created_success_ = success;
  }

  void OnNetworkProcessDisconnected() override {}

  void CrashNetworkProcess(const base::Location& location) override {}

  std::optional<bool> desktop_environment_created_success() const {
    return desktop_environment_created_success_;
  }

  base::WeakPtr<Delegate> GetWeakPtr() { return weak_ptr_.GetWeakPtr(); }

 private:
  std::unique_ptr<DesktopEnvironmentFactory> factory_;
  std::optional<bool> desktop_environment_created_success_;

  base::WeakPtrFactory<FakeDelegate> weak_ptr_{this};
};

class FakeListener : public IPC::Listener {
 public:
  explicit FakeListener(
      base::RepeatingClosure action_after_received,
      mojom::DesktopSessionStateHandler* state_handler = nullptr,
      base::OnceClosure on_state_handler_bound = base::DoNothing(),
      mojom::DesktopSessionEventHandler* event_handler = nullptr)
      : action_after_received_(action_after_received),
        on_state_handler_bound_(std::move(on_state_handler_bound)) {
    if (state_handler) {
      state_handler_receiver_.emplace(state_handler);
    }
    if (event_handler) {
      event_handler_receiver_.emplace(event_handler);
    }
  }
  ~FakeListener() override = default;

 private:
  // IPC::Listener implementation.
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  const base::RepeatingClosure action_after_received_;
  base::OnceClosure on_state_handler_bound_;
  std::optional<mojo::AssociatedReceiver<mojom::DesktopSessionStateHandler>>
      state_handler_receiver_;
  std::optional<mojo::AssociatedReceiver<mojom::DesktopSessionEventHandler>>
      event_handler_receiver_;
};

void FakeListener::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::DesktopSessionStateHandler::Name_) {
    if (state_handler_receiver_.has_value()) {
      state_handler_receiver_->Bind(
          mojo::PendingAssociatedReceiver<mojom::DesktopSessionStateHandler>(
              std::move(handle)));
      state_handler_receiver_->reset_on_disconnect();
      if (on_state_handler_bound_) {
        std::move(on_state_handler_bound_).Run();
      }
    }
    return;
  }
  EXPECT_EQ(mojom::DesktopSessionEventHandler::Name_, interface_name);
  if (event_handler_receiver_.has_value()) {
    event_handler_receiver_->Bind(
        mojo::PendingAssociatedReceiver<mojom::DesktopSessionEventHandler>(
            std::move(handle)));
    event_handler_receiver_->reset_on_disconnect();
  }
  action_after_received_.Run();
}

}  // namespace

class DesktopSessionAgentTest : public ::testing::Test {
 public:
  DesktopSessionAgentTest();
  ~DesktopSessionAgentTest() override;

  void Shutdown();

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::TestFuture<void> shutdown_future_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_refptr<DesktopSessionAgent> agent_;
};

DesktopSessionAgentTest::DesktopSessionAgentTest()
    : task_runner_(
          new AutoThreadTaskRunner(task_environment_.GetMainThreadTaskRunner(),
                                   shutdown_future_.GetCallback())),
      agent_(new DesktopSessionAgent(task_runner_,
                                     task_runner_,
                                     task_runner_,
                                     task_runner_)) {}

DesktopSessionAgentTest::~DesktopSessionAgentTest() = default;

void DesktopSessionAgentTest::Shutdown() {
  task_runner_ = nullptr;
  if (agent_) {
    agent_->Stop();
    agent_ = nullptr;
  }
}

TEST_F(DesktopSessionAgentTest, StartDesktopSessionAgent) {
  std::unique_ptr<FakeDelegate> delegate(new FakeDelegate(task_runner_));
  std::unique_ptr<IPC::ChannelProxy> proxy;
  bool session_started = false;
  FakeListener listener(base::BindLambdaForTesting([&]() {
    session_started = true;
    EXPECT_EQ(delegate->desktop_environment_created_success(), true);
    Shutdown();
    delegate.reset();
    proxy.reset();
  }));
  proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()), IPC::Channel::MODE_CLIENT,
      &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);
  // Let the IPC machinery finish up the interface request before using it.
  task_environment_.RunUntilIdle();

  bool remote_received = false;
  desktop_session_agent->Start(
      "jid", ScreenResolution(), DesktopEnvironmentOptions(),
      base::BindLambdaForTesting(
          [&](mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
                  pending_remote) {
            // Indicate that we received the desktop_session_control remote.
            remote_received = true;
            // Release any references to the other IPC classes.
            desktop_session_agent.reset();
          }));

  EXPECT_TRUE(shutdown_future_.Wait());

  ASSERT_TRUE(remote_received);
  ASSERT_TRUE(session_started);
}

TEST_F(DesktopSessionAgentTest, CreateDesktopEnvironmentFails) {
  DesktopEnvironmentFactory::CreateCallback pending_callback;
  base::test::TestFuture<void> create_called_future;
  auto factory = std::make_unique<MockDesktopEnvironmentFactory>();
  EXPECT_CALL(*factory, Create(_, _, _, _))
      .WillOnce([&](base::WeakPtr<ClientSessionControl>,
                    base::WeakPtr<ClientSessionEvents>,
                    const DesktopEnvironmentOptions&,
                    DesktopEnvironmentFactory::CreateCallback callback) {
        pending_callback = std::move(callback);
        create_called_future.SetValue();
      });
  auto delegate = std::make_unique<FakeDelegate>(std::move(factory));
  MockDesktopSessionStateHandler state_handler;
  base::test::TestFuture<void> state_handler_bound_future;
  base::test::TestFuture<void> disconnected_future;
  base::test::TestFuture<void> agent_disconnected_future;
  EXPECT_CALL(state_handler,
              DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR, _, _))
      .WillOnce([&]() {
        EXPECT_FALSE(agent_disconnected_future.IsReady());
        disconnected_future.SetValue();
      });

  FakeListener listener(base::DoNothing(), &state_handler,
                        state_handler_bound_future.GetCallback());
  std::unique_ptr<IPC::ChannelProxy> proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()), IPC::Channel::MODE_CLIENT,
      &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);

  // `GetRemoteAssociatedInterface()` posts the interface request to
  // `task_runner_`; flush it before calling `Start()` on the same channel.
  base::test::TestFuture<void> ipc_flush_future;
  task_runner_->PostTask(FROM_HERE, ipc_flush_future.GetCallback());
  ASSERT_TRUE(ipc_flush_future.Wait());

  desktop_session_agent.set_disconnect_handler(
      agent_disconnected_future.GetCallback());

  bool start_callback_invoked = false;
  desktop_session_agent->Start(
      "jid", ScreenResolution(), DesktopEnvironmentOptions(),
      base::BindLambdaForTesting(
          [&](mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
                  pending_remote) { start_callback_invoked = true; }));

  ASSERT_TRUE(create_called_future.Wait());
  ASSERT_TRUE(state_handler_bound_future.Wait());
  std::move(pending_callback).Run(nullptr);

  EXPECT_TRUE(disconnected_future.Wait());
  EXPECT_TRUE(agent_disconnected_future.Wait());
  EXPECT_FALSE(start_callback_invoked);
  EXPECT_EQ(delegate->desktop_environment_created_success(), false);

  desktop_session_agent.reset();
  Shutdown();
  delegate.reset();
  proxy.reset();
  EXPECT_TRUE(shutdown_future_.Wait());
}

TEST_F(DesktopSessionAgentTest, StopWhileCreateDesktopEnvironmentPending) {
  DesktopEnvironmentFactory::CreateCallback pending_callback;
  base::test::TestFuture<void> create_called_future;
  auto factory = std::make_unique<MockDesktopEnvironmentFactory>();
  EXPECT_CALL(*factory, Create(_, _, _, _))
      .WillOnce([&](base::WeakPtr<ClientSessionControl>,
                    base::WeakPtr<ClientSessionEvents>,
                    const DesktopEnvironmentOptions&,
                    DesktopEnvironmentFactory::CreateCallback callback) {
        pending_callback = std::move(callback);
        create_called_future.SetValue();
      });
  auto delegate = std::make_unique<FakeDelegate>(std::move(factory));
  FakeListener listener(base::DoNothing());
  std::unique_ptr<IPC::ChannelProxy> proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()), IPC::Channel::MODE_CLIENT,
      &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);

  // `GetRemoteAssociatedInterface()` posts the interface request to
  // `task_runner_`; flush it before calling `Start()` on the same channel.
  base::test::TestFuture<void> ipc_flush_future;
  task_runner_->PostTask(FROM_HERE, ipc_flush_future.GetCallback());
  ASSERT_TRUE(ipc_flush_future.Wait());

  bool start_callback_invoked = false;
  desktop_session_agent->Start(
      "jid", ScreenResolution(), DesktopEnvironmentOptions(),
      base::BindLambdaForTesting(
          [&](mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
                  pending_remote) { start_callback_invoked = true; }));
  ASSERT_TRUE(create_called_future.Wait());
  ASSERT_TRUE(pending_callback);

  Shutdown();
  std::move(pending_callback).Run(nullptr);

  EXPECT_EQ(delegate->desktop_environment_created_success(), std::nullopt);
  desktop_session_agent.reset();
  delegate.reset();
  proxy.reset();
  EXPECT_TRUE(shutdown_future_.Wait());
  EXPECT_FALSE(start_callback_invoked);
}

TEST_F(DesktopSessionAgentTest, NullUrlForwarderConfigurator) {
  DesktopEnvironmentFactory::CreateCallback pending_callback;
  base::test::TestFuture<void> create_called_future;
  auto factory = std::make_unique<NiceMock<MockDesktopEnvironmentFactory>>();
  ON_CALL(*factory, SupportsAudioCapture()).WillByDefault(Return(false));
  EXPECT_CALL(*factory, Create(_, _, _, _))
      .WillOnce([&](base::WeakPtr<ClientSessionControl>,
                    base::WeakPtr<ClientSessionEvents>,
                    const DesktopEnvironmentOptions&,
                    DesktopEnvironmentFactory::CreateCallback callback) {
        pending_callback = std::move(callback);
        create_called_future.SetValue();
      });
  auto delegate = std::make_unique<FakeDelegate>(std::move(factory));
  NiceMock<MockDesktopSessionEventHandler> event_handler;
  base::test::TestFuture<mojom::UrlForwarderState> setup_state_future;
  EXPECT_CALL(event_handler, OnUrlForwarderStateChange(_))
      .WillOnce(base::test::InvokeFuture(setup_state_future));

  base::test::TestFuture<void> event_handler_bound_future;
  FakeListener listener(event_handler_bound_future.GetRepeatingCallback(),
                        /*state_handler=*/nullptr, base::DoNothing(),
                        &event_handler);
  std::unique_ptr<IPC::ChannelProxy> proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()), IPC::Channel::MODE_CLIENT,
      &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);

  // `GetRemoteAssociatedInterface()` posts the interface request to
  // `task_runner_`; flush it before calling `Start()` on the same channel.
  base::test::TestFuture<void> ipc_flush_future;
  task_runner_->PostTask(FROM_HERE, ipc_flush_future.GetCallback());
  ASSERT_TRUE(ipc_flush_future.Wait());

  base::test::TestFuture<
      mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>>
      start_future;
  DesktopEnvironmentOptions options;
  desktop_session_agent->Start("jid", ScreenResolution(), options,
                               start_future.GetCallback());

  ASSERT_TRUE(create_called_future.Wait());
  ASSERT_TRUE(event_handler_bound_future.Wait());
  std::move(pending_callback)
      .Run(std::make_unique<NullUrlForwarderDesktopEnvironment>(task_runner_,
                                                                options));

  mojo::AssociatedRemote<mojom::DesktopSessionControl> desktop_session_control(
      start_future.Take());
  desktop_session_control->SetUpUrlForwarder();
  EXPECT_EQ(setup_state_future.Get(), mojom::UrlForwarderState::kFailed);

  desktop_session_control.reset();
  desktop_session_agent.reset();
  Shutdown();
  delegate.reset();
  proxy.reset();
  EXPECT_TRUE(shutdown_future_.Wait());
}

TEST_F(DesktopSessionAgentTest, NullActionExecutorIgnoresActions) {
  DesktopEnvironmentFactory::CreateCallback pending_callback;
  base::test::TestFuture<void> create_called_future;
  auto factory = std::make_unique<NiceMock<MockDesktopEnvironmentFactory>>();
  ON_CALL(*factory, SupportsAudioCapture()).WillByDefault(Return(false));
  EXPECT_CALL(*factory, Create(_, _, _, _))
      .WillOnce([&](base::WeakPtr<ClientSessionControl>,
                    base::WeakPtr<ClientSessionEvents>,
                    const DesktopEnvironmentOptions&,
                    DesktopEnvironmentFactory::CreateCallback callback) {
        pending_callback = std::move(callback);
        create_called_future.SetValue();
      });
  auto delegate = std::make_unique<FakeDelegate>(std::move(factory));
  NiceMock<MockDesktopSessionEventHandler> event_handler;
  base::test::TestFuture<mojom::UrlForwarderState> setup_state_future;
  EXPECT_CALL(event_handler, OnUrlForwarderStateChange(_))
      .WillOnce(base::test::InvokeFuture(setup_state_future));
  base::test::TestFuture<void> event_handler_bound_future;
  FakeListener listener(event_handler_bound_future.GetRepeatingCallback(),
                        /*state_handler=*/nullptr, base::DoNothing(),
                        &event_handler);
  std::unique_ptr<IPC::ChannelProxy> proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()), IPC::Channel::MODE_CLIENT,
      &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);

  base::test::TestFuture<void> ipc_flush_future;
  task_runner_->PostTask(FROM_HERE, ipc_flush_future.GetCallback());
  ASSERT_TRUE(ipc_flush_future.Wait());

  base::test::TestFuture<
      mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>>
      start_future;
  DesktopEnvironmentOptions options;
  desktop_session_agent->Start("jid", ScreenResolution(), options,
                               start_future.GetCallback());

  ASSERT_TRUE(create_called_future.Wait());
  ASSERT_TRUE(event_handler_bound_future.Wait());
  std::move(pending_callback)
      .Run(std::make_unique<NullActionExecutorDesktopEnvironment>(task_runner_,
                                                                  options));

  mojo::AssociatedRemote<mojom::DesktopSessionControl> desktop_session_control(
      start_future.Take());
  desktop_session_control->InjectSendAttentionSequence();
  desktop_session_control->LockWorkstation();
  // Round-trip a subsequent DesktopSessionControl call on the same FIFO pipe
  // to guarantee both actions ran on DesktopSessionAgent before teardown.
  desktop_session_control->SetUpUrlForwarder();
  EXPECT_EQ(setup_state_future.Get(), mojom::UrlForwarderState::kFailed);

  desktop_session_control.reset();
  desktop_session_agent.reset();
  Shutdown();
  delegate.reset();
  proxy.reset();
  EXPECT_TRUE(shutdown_future_.Wait());
}

}  // namespace remoting
