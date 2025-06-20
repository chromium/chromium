// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
#define REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "remoting/base/http_status.h"
#include "remoting/base/oauth_token_info.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/signaling/signal_strategy.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

namespace apis::v1 {
class GetManagedChromeOsHostResponse;
class HostInfo;
}  // namespace apis::v1

class DirectoryServiceClient;
class OAuthTokenGetter;
class ClientStatusObserver;

namespace protocol {
class ConnectionToHost;
class FrameConsumer;
class SessionManager;
class VideoRenderer;
}  // namespace protocol

// A simple, native chromoting client implementation.
class RemotingClient : public SignalStrategy::Listener,
                       public protocol::ConnectionToHost::HostEventCallback,
                       public protocol::ClientStub {
 public:
  RemotingClient(
      base::OnceClosure quit_closure,
      protocol::FrameConsumer* frame_consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RemotingClient(const RemotingClient&) = delete;
  RemotingClient& operator=(const RemotingClient&) = delete;

  ~RemotingClient() override;

  void StartSession(std::string_view support_access_code,
                    OAuthTokenInfo oauth_token_info);

  void StopSession();

  void AddObserver(ClientStatusObserver* observer);
  void RemoveObserver(ClientStatusObserver* observer);

 private:
  // ClientStub implementation.
  void SetCapabilities(const protocol::Capabilities& capabilities) override;
  void SetPairingResponse(
      const protocol::PairingResponse& pairing_response) override;
  void DeliverHostMessage(const protocol::ExtensionMessage& message) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;
  void SetTransportInfo(const protocol::TransportInfo& transport_info) override;
  void SetActiveDisplay(const protocol::ActiveDisplay& active_display) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;
  void SetKeyboardLayout(const protocol::KeyboardLayout& layout) override;

  // ConnectionToHost::HostEventCallback implementation.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override;
  void OnConnectionReady(bool ready) override;
  void OnRouteChanged(const std::string& channel_name,
                      const protocol::TransportRoute& route) override;

  // SignalStrategy::StatusObserver interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void OnGetManagedChromeOsHostRetrieved(
      const HttpStatus& status,
      std::unique_ptr<apis::v1::GetManagedChromeOsHostResponse> response);

  void StartConnection();
  void RunQuitClosure();

  std::string host_id_;
  std::string host_secret_;
  OAuthTokenInfo oauth_token_info_;
  base::OnceClosure quit_closure_;
  base::ObserverList<ClientStatusObserver> observers_;

  // Used to provide an OAuth access token for service requests. Since a raw *
  // is passed around, this field should be destroyed after the service clients.
  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;

  // Used to retrieve details about the remote host to connect to.
  std::unique_ptr<DirectoryServiceClient> directory_service_client_;

  // Information about the remote host being connected to.
  std::unique_ptr<apis::v1::HostInfo> chrome_os_host_;

  // TODO: joedow - |Move FtlSignalingConnector| from //remoting/host into
  // //remoting/signaling so it can be used in the client.
  std::unique_ptr<SignalStrategy> signal_strategy_;

  // |frame_consumer_| must outlive |video_renderer_|.
  const raw_ptr<protocol::FrameConsumer> frame_consumer_;

  // Session related members.
  std::unique_ptr<protocol::ConnectionToHost> connection_;
  std::unique_ptr<protocol::SessionManager> session_manager_;
  std::unique_ptr<protocol::VideoRenderer> video_renderer_;

  // Used to make service requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
