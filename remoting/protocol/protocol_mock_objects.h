// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/errors.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/session_observer.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting::protocol {

class MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator();

  MockAuthenticator(const MockAuthenticator&) = delete;
  MockAuthenticator& operator=(const MockAuthenticator&) = delete;

  ~MockAuthenticator() override;

  MOCK_METHOD(CredentialsType, credentials_type, (), (const, override));
  MOCK_METHOD(const Authenticator&,
              implementing_authenticator,
              (),
              (const, override));
  MOCK_METHOD(const SessionPolicies*,
              GetSessionPolicies,
              (),
              (const, override));
  MOCK_METHOD(Authenticator::State, state, (), (const, override));
  MOCK_METHOD(bool, started, (), (const, override));
  MOCK_METHOD(Authenticator::RejectionReason,
              rejection_reason,
              (),
              (const, override));
  MOCK_METHOD(Authenticator::RejectionDetails,
              rejection_details,
              (),
              (const, override));
  MOCK_METHOD(const std::string&, GetAuthKey, (), (const, override));
  MOCK_METHOD(void,
              ProcessMessage,
              (const JingleAuthentication& message,
               base::OnceClosure resume_callback),
              (override));
  MOCK_METHOD(JingleAuthentication, GetNextMessage, (), (override));

  // Make this method public.
  void NotifyStateChangeAfterAccepted() override {
    Authenticator::NotifyStateChangeAfterAccepted();
  }
};

class MockConnectionToClientEventHandler
    : public ConnectionToClient::EventHandler {
 public:
  MockConnectionToClientEventHandler();

  MockConnectionToClientEventHandler(
      const MockConnectionToClientEventHandler&) = delete;
  MockConnectionToClientEventHandler& operator=(
      const MockConnectionToClientEventHandler&) = delete;

  ~MockConnectionToClientEventHandler() override;

  MOCK_METHOD(void, OnConnectionAuthenticating, (), (override));
  MOCK_METHOD(void,
              OnConnectionAuthenticated,
              (const SessionPolicies*),
              (override));
  MOCK_METHOD(void, CreateMediaStreams, (), (override));
  MOCK_METHOD(void, OnConnectionChannelsConnected, (), (override));
  MOCK_METHOD(void, OnConnectionClosed, (ErrorCode error), (override));
  MOCK_METHOD(void,
              OnTransportProtocolChange,
              (const std::string& protocol),
              (override));
  MOCK_METHOD(void,
              OnRouteChange,
              (const std::string& channel_name, const TransportRoute& route),
              (override));
  MOCK_METHOD(void,
              OnIncomingAudioFormatChanged,
              (const AudioSampleInfo&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              OnIncomingDataChannelPtr,
              (const std::string& channel_name, MessagePipe* pipe));
  void OnIncomingDataChannel(const std::string& channel_name,
                             std::unique_ptr<MessagePipe> pipe) override {
    OnIncomingDataChannelPtr(channel_name, pipe.get());
  }
};

class MockClipboardStub : public ClipboardStub {
 public:
  MockClipboardStub();

  MockClipboardStub(const MockClipboardStub&) = delete;
  MockClipboardStub& operator=(const MockClipboardStub&) = delete;

  ~MockClipboardStub() override;

  MOCK_METHOD(void,
              InjectClipboardEvent,
              (const ClipboardEvent& event),
              (override));
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();

  MockInputStub(const MockInputStub&) = delete;
  MockInputStub& operator=(const MockInputStub&) = delete;

  ~MockInputStub() override;

  MOCK_METHOD(void, InjectKeyEvent, (const KeyEvent& event), (override));
  MOCK_METHOD(void, InjectTextEvent, (const TextEvent& event), (override));
  MOCK_METHOD(void, InjectMouseEvent, (const MouseEvent& event), (override));
  MOCK_METHOD(void, InjectTouchEvent, (const TouchEvent& event), (override));
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();

  MockHostStub(const MockHostStub&) = delete;
  MockHostStub& operator=(const MockHostStub&) = delete;

  ~MockHostStub() override;

  MOCK_METHOD(void,
              NotifyClientResolution,
              (const ClientResolution& resolution),
              (override));
  MOCK_METHOD(void,
              ControlVideo,
              (const VideoControl& video_control),
              (override));
  MOCK_METHOD(void,
              ControlAudio,
              (const AudioControl& audio_control),
              (override));
  MOCK_METHOD(void,
              ControlPeerConnection,
              (const PeerConnectionParameters& parameters),
              (override));
  MOCK_METHOD(void,
              SetCapabilities,
              (const Capabilities& capabilities),
              (override));
  MOCK_METHOD(void,
              RequestPairing,
              (const PairingRequest& pairing_request),
              (override));
  MOCK_METHOD(void,
              DeliverClientMessage,
              (const ExtensionMessage& message),
              (override));
  MOCK_METHOD(void,
              SelectDesktopDisplay,
              (const SelectDesktopDisplayRequest& message),
              (override));
  MOCK_METHOD(void,
              SetVideoLayout,
              (const VideoLayout& video_layout),
              (override));
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();

  MockClientStub(const MockClientStub&) = delete;
  MockClientStub& operator=(const MockClientStub&) = delete;

  ~MockClientStub() override;

  // ClientStub mock implementation.
  MOCK_METHOD(void,
              SetCapabilities,
              (const Capabilities& capabilities),
              (override));
  MOCK_METHOD(void,
              SetPairingResponse,
              (const PairingResponse& pairing_response),
              (override));
  MOCK_METHOD(void,
              DeliverHostMessage,
              (const ExtensionMessage& message),
              (override));
  MOCK_METHOD(void, SetVideoLayout, (const VideoLayout& layout), (override));
  MOCK_METHOD(void,
              SetTransportInfo,
              (const TransportInfo& transport_info),
              (override));
  MOCK_METHOD(void,
              SetActiveDisplay,
              (const ActiveDisplay& active_display),
              (override));
  MOCK_METHOD(void,
              ControlMicrophone,
              (const MicrophoneControl& control),
              (override));

  // ClipboardStub mock implementation.
  MOCK_METHOD(void,
              InjectClipboardEvent,
              (const ClipboardEvent& event),
              (override));

  // CursorShapeStub mock implementation.
  MOCK_METHOD(void,
              SetCursorShape,
              (const CursorShapeInfo& cursor_shape),
              (override));
  MOCK_METHOD(void,
              SetHostCursorPosition,
              (const HostCursorPosition& position),
              (override));

  // KeyboardLayoutStub mock implementation.
  MOCK_METHOD(void,
              SetKeyboardLayout,
              (const KeyboardLayout& layout),
              (override));
};

class MockCursorShapeStub : public CursorShapeStub {
 public:
  MockCursorShapeStub();

  MockCursorShapeStub(const MockCursorShapeStub&) = delete;
  MockCursorShapeStub& operator=(const MockCursorShapeStub&) = delete;

  ~MockCursorShapeStub() override;

  MOCK_METHOD(void,
              SetCursorShape,
              (const CursorShapeInfo& cursor_shape),
              (override));
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();

  MockVideoStub(const MockVideoStub&) = delete;
  MockVideoStub& operator=(const MockVideoStub&) = delete;

  ~MockVideoStub() override;

  MOCK_METHOD(void,
              ProcessVideoPacketPtr,
              (const VideoPacket* video_packet, base::OnceClosure* done));
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override {
    ProcessVideoPacketPtr(video_packet.get(), &done);
  }
};

class MockSession : public Session {
 public:
  MockSession();

  MockSession(const MockSession&) = delete;
  MockSession& operator=(const MockSession&) = delete;

  ~MockSession() override;

  MOCK_METHOD(void,
              SetEventHandler,
              (Session::EventHandler * event_handler),
              (override));
  MOCK_METHOD(ErrorCode, error, (), (const, override));
  MOCK_METHOD(void, SetTransport, (Transport*), (override));
  MOCK_METHOD(const std::string&, jid, (), (override));
  MOCK_METHOD(const Authenticator&, authenticator, (), (const, override));
  MOCK_METHOD(void,
              Close,
              (ErrorCode error,
               std::string_view error_details,
               const SourceLocation& location),
              (override));
  MOCK_METHOD(void, AddPlugin, (SessionPlugin * plugin), (override));
};

class MockSessionManager : public SessionManager {
 public:
  MockSessionManager();

  MockSessionManager(const MockSessionManager&) = delete;
  MockSessionManager& operator=(const MockSessionManager&) = delete;

  ~MockSessionManager() override;

  MOCK_METHOD(void,
              AcceptIncoming,
              (const IncomingSessionCallback&),
              (override));
  MOCK_METHOD(Session*,
              ConnectPtr,
              (const SignalingAddress& peer_address,
               Authenticator* authenticator));
  MOCK_METHOD(void, Close, ());
  MOCK_METHOD(void,
              set_authenticator_factory_ptr,
              (AuthenticatorFactory * factory));
  MOCK_METHOD(SessionObserver::Subscription,
              AddSessionObserver,
              (SessionObserver * observer),
              (override));
  std::unique_ptr<Session> Connect(
      const SignalingAddress& peer_address,
      std::unique_ptr<Authenticator> authenticator) override {
    return base::WrapUnique(ConnectPtr(peer_address, authenticator.get()));
  }
  void set_authenticator_factory(
      std::unique_ptr<AuthenticatorFactory> authenticator_factory) override {
    set_authenticator_factory_ptr(authenticator_factory.release());
  }
};

class MockSessionObserver : public SessionObserver {
 public:
  MockSessionObserver();
  ~MockSessionObserver() override;

  MockSessionObserver(const MockSessionObserver&) = delete;
  MockSessionObserver& operator=(const MockSessionObserver&) = delete;

  MOCK_METHOD(void, OnSessionStateChange, (const Session&, Session::State));
};

// Simple delegate that caches information on paired clients in memory.
class MockPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  MockPairingRegistryDelegate();
  ~MockPairingRegistryDelegate() override;

  // PairingRegistry::Delegate implementation.
  base::ListValue LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

 private:
  typedef std::map<std::string, protocol::PairingRegistry::Pairing> Pairings;
  Pairings pairings_;
};

class SynchronousPairingRegistry : public PairingRegistry {
 public:
  explicit SynchronousPairingRegistry(std::unique_ptr<Delegate> delegate);

 protected:
  ~SynchronousPairingRegistry() override;

  // Runs tasks synchronously instead of posting them to |task_runner|.
  void PostTask(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                const base::Location& from_here,
                base::OnceClosure task) override;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
