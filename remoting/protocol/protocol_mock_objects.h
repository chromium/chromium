// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
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
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

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
  MOCK_CONST_METHOD0(state, Authenticator::State());
  MOCK_CONST_METHOD0(started, bool());
  MOCK_CONST_METHOD0(rejection_reason, Authenticator::RejectionReason());
  MOCK_CONST_METHOD0(GetAuthKey, const std::string&());
  MOCK_CONST_METHOD0(CreateChannelAuthenticatorPtr, ChannelAuthenticator*());
  MOCK_METHOD2(ProcessMessage,
               void(const jingle_xmpp::XmlElement* message,
                    base::OnceClosure resume_callback));
  MOCK_METHOD0(GetNextMessagePtr, jingle_xmpp::XmlElement*());

  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override {
    return base::WrapUnique(CreateChannelAuthenticatorPtr());
  }

  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override {
    return base::WrapUnique(GetNextMessagePtr());
  }

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

  MOCK_METHOD0(OnConnectionAuthenticating, void());
  MOCK_METHOD(void,
              OnConnectionAuthenticated,
              (const SessionPolicies*),
              (override));
  MOCK_METHOD0(CreateMediaStreams, void());
  MOCK_METHOD0(OnConnectionChannelsConnected, void());
  MOCK_METHOD1(OnConnectionClosed, void(ErrorCode error));
  MOCK_METHOD1(OnTransportProtocolChange, void(const std::string& protocol));
  MOCK_METHOD2(OnRouteChange,
               void(const std::string& channel_name,
                    const TransportRoute& route));

  MOCK_METHOD2(OnIncomingDataChannelPtr,
               void(const std::string& channel_name, MessagePipe* pipe));
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

  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();

  MockInputStub(const MockInputStub&) = delete;
  MockInputStub& operator=(const MockInputStub&) = delete;

  ~MockInputStub() override;

  MOCK_METHOD1(InjectKeyEvent, void(const KeyEvent& event));
  MOCK_METHOD1(InjectTextEvent, void(const TextEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const MouseEvent& event));
  MOCK_METHOD1(InjectTouchEvent, void(const TouchEvent& event));
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();

  MockHostStub(const MockHostStub&) = delete;
  MockHostStub& operator=(const MockHostStub&) = delete;

  ~MockHostStub() override;

  MOCK_METHOD1(NotifyClientResolution,
               void(const ClientResolution& resolution));
  MOCK_METHOD1(ControlVideo, void(const VideoControl& video_control));
  MOCK_METHOD1(ControlAudio, void(const AudioControl& audio_control));
  MOCK_METHOD1(ControlPeerConnection,
               void(const PeerConnectionParameters& parameters));
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(RequestPairing, void(const PairingRequest& pairing_request));
  MOCK_METHOD1(DeliverClientMessage, void(const ExtensionMessage& message));
  MOCK_METHOD1(SelectDesktopDisplay,
               void(const SelectDesktopDisplayRequest& message));
  MOCK_METHOD1(SetVideoLayout, void(const VideoLayout& video_layout));
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();

  MockClientStub(const MockClientStub&) = delete;
  MockClientStub& operator=(const MockClientStub&) = delete;

  ~MockClientStub() override;

  // ClientStub mock implementation.
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(SetPairingResponse,
               void(const PairingResponse& pairing_response));
  MOCK_METHOD1(DeliverHostMessage, void(const ExtensionMessage& message));
  MOCK_METHOD1(SetVideoLayout, void(const VideoLayout& layout));
  MOCK_METHOD1(SetTransportInfo, void(const TransportInfo& transport_info));
  MOCK_METHOD1(SetActiveDisplay, void(const ActiveDisplay& active_display));

  // ClipboardStub mock implementation.
  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

  // CursorShapeStub mock implementation.
  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

  // KeyboardLayoutStub mock implementation.
  MOCK_METHOD1(SetKeyboardLayout, void(const KeyboardLayout& layout));
};

class MockCursorShapeStub : public CursorShapeStub {
 public:
  MockCursorShapeStub();

  MockCursorShapeStub(const MockCursorShapeStub&) = delete;
  MockCursorShapeStub& operator=(const MockCursorShapeStub&) = delete;

  ~MockCursorShapeStub() override;

  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();

  MockVideoStub(const MockVideoStub&) = delete;
  MockVideoStub& operator=(const MockVideoStub&) = delete;

  ~MockVideoStub() override;

  MOCK_METHOD2(ProcessVideoPacketPtr,
               void(const VideoPacket* video_packet, base::OnceClosure* done));
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

  MOCK_METHOD1(SetEventHandler, void(Session::EventHandler* event_handler));
  MOCK_METHOD(ErrorCode, error, (), (const, override));
  MOCK_METHOD1(SetTransport, void(Transport*));
  MOCK_METHOD0(jid, const std::string&());
  MOCK_METHOD0(config, const SessionConfig&());
  MOCK_METHOD(const Authenticator&, authenticator, (), (const, override));
  MOCK_METHOD1(Close, void(ErrorCode error));
  MOCK_METHOD1(AddPlugin, void(SessionPlugin* plugin));
};

class MockSessionManager : public SessionManager {
 public:
  MockSessionManager();

  MockSessionManager(const MockSessionManager&) = delete;
  MockSessionManager& operator=(const MockSessionManager&) = delete;

  ~MockSessionManager() override;

  MOCK_METHOD1(AcceptIncoming, void(const IncomingSessionCallback&));
  void set_protocol_config(
      std::unique_ptr<CandidateSessionConfig> config) override {}
  MOCK_METHOD2(ConnectPtr,
               Session*(const SignalingAddress& peer_address,
                        Authenticator* authenticator));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(set_authenticator_factory_ptr,
               void(AuthenticatorFactory* factory));
  MOCK_METHOD(SessionObserver::Subscription,
              AddSessionObserver,
              (SessionObserver * observer));
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
  base::Value::List LoadAll() override;
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
