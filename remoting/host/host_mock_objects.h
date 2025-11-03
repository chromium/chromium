// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/base/errors.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/chromoting_host_services_provider.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/client_session_events.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/events/types/event_type.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace remoting {

class MockDesktopEnvironment : public DesktopEnvironment {
 public:
  MockDesktopEnvironment();
  ~MockDesktopEnvironment() override;

  MOCK_METHOD(std::unique_ptr<ActionExecutor>,
              CreateActionExecutor,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<AudioCapturer>,
              CreateAudioCapturer,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<InputInjector>,
              CreateInputInjector,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<ScreenControls>,
              CreateScreenControls,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<DesktopCapturer>,
              CreateVideoCapturer,
              (webrtc::ScreenId),
              (override));
  MOCK_METHOD(DesktopDisplayInfoMonitor*,
              GetDisplayInfoMonitor,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<protocol::MouseCursorMonitor>,
              CreateMouseCursorMonitor,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<KeyboardLayoutMonitor>,
              CreateKeyboardLayoutMonitor,
              (base::RepeatingCallback<void(const protocol::KeyboardLayout&)>),
              (override));
  MOCK_METHOD(std::unique_ptr<ActiveDisplayMonitor>,
              CreateActiveDisplayMonitor,
              (base::RepeatingCallback<void(webrtc::ScreenId)>),
              (override));
  MOCK_METHOD(std::unique_ptr<FileOperations>,
              CreateFileOperations,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<UrlForwarderConfigurator>,
              CreateUrlForwarderConfigurator,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<RemoteWebAuthnStateChangeNotifier>,
              CreateRemoteWebAuthnStateChangeNotifier,
              (),
              (override));
  MOCK_METHOD(std::string, GetCapabilities, (), (const, override));
  MOCK_METHOD(void, SetCapabilities, (const std::string&), (override));
  MOCK_METHOD(std::uint32_t, GetDesktopSessionId, (), (const, override));
};

class MockClientSessionControl : public ClientSessionControl {
 public:
  MockClientSessionControl();

  MockClientSessionControl(const MockClientSessionControl&) = delete;
  MockClientSessionControl& operator=(const MockClientSessionControl&) = delete;

  ~MockClientSessionControl() override;

  MOCK_METHOD(const std::string&, client_jid, (), (const, override));
  MOCK_METHOD(void,
              DisconnectSession,
              (ErrorCode error,
               std::string_view error_details,
               const SourceLocation& error_location),
              (override));
  MOCK_METHOD(void,
              OnLocalPointerMoved,
              (const webrtc::DesktopVector&, ui::EventType),
              (override));
  MOCK_METHOD(void, OnLocalKeyPressed, (std::uint32_t), (override));
  MOCK_METHOD(void, SetDisableInputs, (bool), (override));
  MOCK_METHOD(void,
              OnDesktopDisplayChanged,
              (std::unique_ptr<protocol::VideoLayout>),
              (override));
};

class MockClientSessionDetails : public ClientSessionDetails {
 public:
  MockClientSessionDetails();

  MockClientSessionDetails(const MockClientSessionDetails&) = delete;
  MockClientSessionDetails& operator=(const MockClientSessionDetails&) = delete;

  ~MockClientSessionDetails() override;

  MOCK_METHOD(ClientSessionControl*, session_control, (), (override));
  MOCK_METHOD(std::uint32_t, desktop_session_id, (), (const, override));
};

class MockClientSessionEvents : public ClientSessionEvents {
 public:
  MockClientSessionEvents();
  ~MockClientSessionEvents() override;

  MOCK_METHOD(void, OnDesktopAttached, (std::uint32_t session_id), (override));
  MOCK_METHOD(void, OnDesktopDetached, (), (override));
};

class MockClientSessionEventHandler : public ClientSession::EventHandler {
 public:
  MockClientSessionEventHandler();

  MockClientSessionEventHandler(const MockClientSessionEventHandler&) = delete;
  MockClientSessionEventHandler& operator=(
      const MockClientSessionEventHandler&) = delete;

  ~MockClientSessionEventHandler() override;

  MOCK_METHOD(void, OnSessionAuthenticating, (ClientSession*), (override));
  MOCK_METHOD(void, OnSessionAuthenticated, (ClientSession*), (override));
  MOCK_METHOD(void,
              OnSessionChannelsConnected,
              (ClientSession * client),
              (override));
  MOCK_METHOD(void,
              OnSessionAuthenticationFailed,
              (ClientSession * client),
              (override));
  MOCK_METHOD(void, OnSessionClosed, (ClientSession*), (override));
  MOCK_METHOD(void,
              OnSessionRouteChange,
              (ClientSession*,
               const std::string&,
               const protocol::TransportRoute&),
              (override));
  MOCK_METHOD(std::optional<ErrorCode>,
              OnSessionPoliciesReceived,
              (const SessionPolicies& policies),
              (override));
};

class MockDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  MockDesktopEnvironmentFactory();

  MockDesktopEnvironmentFactory(const MockDesktopEnvironmentFactory&) = delete;
  MockDesktopEnvironmentFactory& operator=(
      const MockDesktopEnvironmentFactory&) = delete;

  ~MockDesktopEnvironmentFactory() override;

  MOCK_METHOD(void,
              Create,
              (base::WeakPtr<ClientSessionControl>,
               base::WeakPtr<ClientSessionEvents>,
               const DesktopEnvironmentOptions&,
               CreateCallback),
              (override));
  MOCK_METHOD(bool, SupportsAudioCapture, (), (const, override));
};

class MockInputInjector : public InputInjector {
 public:
  MockInputInjector();

  MockInputInjector(const MockInputInjector&) = delete;
  MockInputInjector& operator=(const MockInputInjector&) = delete;

  ~MockInputInjector() override;

  // protocol::ClipboardStub interface.
  MOCK_METHOD(void,
              InjectClipboardEvent,
              (const protocol::ClipboardEvent&),
              (override));

  // protocol::InputStub interface.
  MOCK_METHOD(void, InjectKeyEvent, (const protocol::KeyEvent&), (override));
  MOCK_METHOD(void, InjectTextEvent, (const protocol::TextEvent&), (override));
  MOCK_METHOD(void,
              InjectMouseEvent,
              (const protocol::MouseEvent&),
              (override));
  MOCK_METHOD(void,
              InjectTouchEvent,
              (const protocol::TouchEvent&),
              (override));

  // InputInjector interface.
  MOCK_METHOD(void,
              Start,
              (std::unique_ptr<protocol::ClipboardStub>),
              (override));
};

class MockHostStatusObserver : public HostStatusObserver {
 public:
  MockHostStatusObserver();

  MockHostStatusObserver(const MockHostStatusObserver&) = delete;
  MockHostStatusObserver& operator=(const MockHostStatusObserver&) = delete;

  ~MockHostStatusObserver() override;

  MOCK_METHOD(void, OnClientAccessDenied, (const std::string&), (override));
  MOCK_METHOD(void, OnClientAuthenticated, (const std::string&), (override));
  MOCK_METHOD(void, OnClientConnected, (const std::string&), (override));
  MOCK_METHOD(void, OnClientDisconnected, (const std::string&), (override));
  MOCK_METHOD(void,
              OnClientRouteChange,
              (const std::string&,
               const std::string&,
               const protocol::TransportRoute&),
              (override));
  MOCK_METHOD(void, OnHostStarted, (const std::string&), (override));
  MOCK_METHOD(void, OnHostShutdown, (), (override));
};

class MockSecurityKeyAuthHandler : public SecurityKeyAuthHandler {
 public:
  MockSecurityKeyAuthHandler();

  MockSecurityKeyAuthHandler(const MockSecurityKeyAuthHandler&) = delete;
  MockSecurityKeyAuthHandler& operator=(const MockSecurityKeyAuthHandler&) =
      delete;

  ~MockSecurityKeyAuthHandler() override;

  MOCK_METHOD(void, CreateSecurityKeyConnection, (), (override));
  MOCK_METHOD(bool, IsValidConnectionId, (int), (const, override));
  MOCK_METHOD(void, SendClientResponse, (int, const std::string&), (override));
  MOCK_METHOD(void, SendErrorAndCloseConnection, (int), (override));
  MOCK_METHOD(std::size_t,
              GetActiveConnectionCountForTest,
              (),
              (const, override));
  MOCK_METHOD(void, SetRequestTimeoutForTest, (base::TimeDelta), (override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              BindSecurityKeyForwarder,
              (mojo::PendingReceiver<mojom::SecurityKeyForwarder>),
              (override));
#endif

  void SetSendMessageCallback(
      const SecurityKeyAuthHandler::SendMessageCallback& callback) override;
  const SecurityKeyAuthHandler::SendMessageCallback& GetSendMessageCallback();

 private:
  SecurityKeyAuthHandler::SendMessageCallback callback_;
};

class MockMouseCursorMonitor : public protocol::MouseCursorMonitor {
 public:
  MockMouseCursorMonitor();

  MockMouseCursorMonitor(const MockMouseCursorMonitor&) = delete;
  MockMouseCursorMonitor& operator=(const MockMouseCursorMonitor&) = delete;

  ~MockMouseCursorMonitor() override;

  MOCK_METHOD(void,
              Init,
              (protocol::MouseCursorMonitor::Callback*),
              (override));
  MOCK_METHOD(void, SetPreferredCaptureInterval, (base::TimeDelta), (override));
};

class MockUrlForwarderConfigurator final : public UrlForwarderConfigurator {
 public:
  MockUrlForwarderConfigurator();

  MockUrlForwarderConfigurator(const MockUrlForwarderConfigurator&) = delete;
  MockUrlForwarderConfigurator& operator=(const MockUrlForwarderConfigurator&) =
      delete;

  ~MockUrlForwarderConfigurator() override;

  MOCK_METHOD(void,
              IsUrlForwarderSetUp,
              (IsUrlForwarderSetUpCallback callback),
              (override));
  MOCK_METHOD(void,
              SetUpUrlForwarder,
              (const SetUpUrlForwarderCallback& callback),
              (override));
};

class MockChromotingSessionServices : public mojom::ChromotingSessionServices {
 public:
  MockChromotingSessionServices();

  MockChromotingSessionServices(const MockChromotingSessionServices&) = delete;
  MockChromotingSessionServices& operator=(
      const MockChromotingSessionServices&) = delete;

  ~MockChromotingSessionServices() override;

  MOCK_METHOD(void,
              BindRemoteUrlOpener,
              (mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver),
              (override));
  MOCK_METHOD(void,
              BindWebAuthnProxy,
              (mojo::PendingReceiver<mojom::WebAuthnProxy> receiver),
              (override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              BindSecurityKeyForwarder,
              (mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver),
              (override));
#endif
};

class MockChromotingHostServicesProvider
    : public ChromotingHostServicesProvider {
 public:
  MockChromotingHostServicesProvider();

  MockChromotingHostServicesProvider(
      const MockChromotingHostServicesProvider&) = delete;
  MockChromotingHostServicesProvider& operator=(
      const MockChromotingHostServicesProvider&) = delete;

  ~MockChromotingHostServicesProvider() override;

  MOCK_METHOD(mojom::ChromotingSessionServices*,
              GetSessionServices,
              (),
              (const, override));
  MOCK_METHOD(void,
              set_disconnect_handler,
              (base::OnceClosure disconnect_handler),
              (override));
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MOCK_OBJECTS_H_
