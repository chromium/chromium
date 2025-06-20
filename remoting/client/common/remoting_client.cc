// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/remoting_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/webrtc/thread_wrapper.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/oauth_token_info.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/client/common/client_status_observer.h"
#include "remoting/client/common/frame_consumer_wrapper.h"
#include "remoting/client/common/logging.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/remoting/v1/host_info.pb.h"
#include "remoting/proto/remoting/v1/remote_support_host_messages.pb.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_config_fetcher_default.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_connection_to_host.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace remoting {

namespace {
constexpr std::int32_t kMinBitrateBps = 10485760;
}

RemotingClient::RemotingClient(
    base::OnceClosure quit_closure,
    protocol::FrameConsumer* frame_consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : quit_closure_(std::move(quit_closure)),
      frame_consumer_(frame_consumer),
      url_loader_factory_(url_loader_factory) {
  CHECK(frame_consumer_);
}

RemotingClient::~RemotingClient() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
  observers_.Notify(&ClientStatusObserver::OnClientDestroyed);
}

void RemotingClient::StartSession(std::string_view support_access_code,
                                  OAuthTokenInfo oauth_token_info) {
  CHECK(!session_manager_);
  CHECK_EQ(support_access_code.length(), 12UL);
  CHECK_GT(oauth_token_info.access_token().length(), 0UL);
  CHECK_GT(oauth_token_info.user_email().length(), 0UL);

  host_id_ = support_access_code.substr(0, 7);
  // Though the host-side impl generates a 5 digit 'secret', the authenticator
  // considers the full 12-digit access code to be the secret.
  host_secret_ = support_access_code;

  // TODO: joedow - If we need to support sessions > 1 hour, we will need to
  // provide a method for refreshing the access token.
  oauth_token_info_ = std::move(oauth_token_info);
  oauth_token_getter_ =
      std::make_unique<PassthroughOAuthTokenGetter>(oauth_token_info_);
  directory_service_client_ = std::make_unique<DirectoryServiceClient>(
      oauth_token_getter_.get(), url_loader_factory_);

  // base::Unretained is sound because this instance owns the service client
  // and callbacks will not be run after destruction.
  CLIENT_LOG << "Retrieving host information for id: " << host_id_;
  directory_service_client_->GetManagedChromeOsHost(
      host_id_,
      base::BindOnce(&RemotingClient::OnGetManagedChromeOsHostRetrieved,
                     base::Unretained(this)));
}

void RemotingClient::OnGetManagedChromeOsHostRetrieved(
    const HttpStatus& status,
    std::unique_ptr<apis::v1::GetManagedChromeOsHostResponse> response) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to retrieve host information. code: "
               << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
    RunQuitClosure();
    return;
  }

  if (!response->has_chrome_os_host()) {
    LOG(ERROR) << "Directory response did not include a chrome_os_host value";
    RunQuitClosure();
    return;
  }

  chrome_os_host_.reset(response->release_chrome_os_host());
  if (!chrome_os_host_->has_ftl_id()) {
    LOG(ERROR) << "Directory response did not include an ftl_id value";
    RunQuitClosure();
    return;
  }

  CLIENT_LOG << "Initializing signaling...";
  signal_strategy_ = std::make_unique<FtlSignalStrategy>(
      std::make_unique<PassthroughOAuthTokenGetter>(oauth_token_info_),
      url_loader_factory_, std::make_unique<FtlClientUuidDeviceIdProvider>());
  signal_strategy_->AddListener(this);
  signal_strategy_->Connect();
}

void RemotingClient::StartConnection() {
  CLIENT_LOG << "Creating transport context...";
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          webrtc::ThreadWrapper::current()->SocketServer(),
          std::make_unique<protocol::IceConfigFetcherDefault>(
              url_loader_factory_, oauth_token_getter_.get()),
          protocol::TransportRole::CLIENT);
  // WebrtcVideoRendererAdapter only supports I420 so we request AV1-Profile0.
  transport_context->set_preferred_video_format(
      webrtc::SdpVideoFormat::AV1Profile0());

  CLIENT_LOG << "Creating session manager...";
  auto protocol_config = protocol::CandidateSessionConfig::CreateDefault();
  protocol_config->DisableAudioChannel();
  protocol_config->set_webrtc_supported(true);
  session_manager_ =
      std::make_unique<protocol::JingleSessionManager>(signal_strategy_.get());
  session_manager_->set_protocol_config(std::move(protocol_config));

  CLIENT_LOG << "Creating session...";
  auto host_signaling_id = NormalizeSignalingId(chrome_os_host_->ftl_id());
  protocol::ClientAuthenticationConfig client_auth_config = {};
  client_auth_config.host_id = host_id_;
  client_auth_config.fetch_secret_callback = base::BindRepeating(
      [](const std::string& secret, bool pairing_supported,
         const protocol::SecretFetchedCallback& secret_fetched_callback) {
        secret_fetched_callback.Run(secret);
      },
      host_secret_);
  auto session = session_manager_->Connect(
      SignalingAddress(host_signaling_id),
      std::make_unique<protocol::NegotiatingClientAuthenticator>(
          signal_strategy_->GetLocalAddress().id(), host_signaling_id,
          std::move(client_auth_config)));

  CLIENT_LOG << "Creating video renderer...";
  video_renderer_ = std::make_unique<FrameConsumerWrapper>(frame_consumer_);

  CLIENT_LOG << "Establishing connection to host...";
  connection_ = std::make_unique<protocol::WebrtcConnectionToHost>();
  connection_->set_client_stub(this);
  connection_->set_clipboard_stub(this);
  connection_->set_video_renderer(video_renderer_.get());
  connection_->Connect(std::move(session), transport_context, this);
  protocol::NetworkSettings network_settings{
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL};
  connection_->ApplyNetworkSettings(network_settings);
}

void RemotingClient::StopSession() {
  CLIENT_LOG << "Shutting down connection to host...";
  if (connection_) {
    connection_->Disconnect(ErrorCode::OK);
    connection_.reset();
  }
  if (signal_strategy_) {
    // Delay tearing down the signaling channel to increase the likelihood that
    // the host processes the connection state change.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SignalStrategy::Disconnect,
                       base::Unretained(signal_strategy_.get())),
        base::Seconds(2));
  }
  return;
}

void RemotingClient::AddObserver(ClientStatusObserver* observer) {
  observers_.AddObserver(observer);
}

void RemotingClient::RemoveObserver(ClientStatusObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RemotingClient::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  NOTIMPLEMENTED();
}

void RemotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetVideoLayout(const protocol::VideoLayout& layout) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetTransportInfo(
    const protocol::TransportInfo& transport_info) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetActiveDisplay(
    const protocol::ActiveDisplay& active_display) {
  NOTIMPLEMENTED();
}

void RemotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  NOTIMPLEMENTED();
}

void RemotingClient::SetKeyboardLayout(const protocol::KeyboardLayout& layout) {
  NOTIMPLEMENTED();
}

void RemotingClient::OnConnectionState(protocol::ConnectionToHost::State state,
                                       protocol::ErrorCode error) {
  CLIENT_LOG << "OnConnectionState change: "
             << protocol::ConnectionToHost::StateToString(state);
  if (state == protocol::ConnectionToHost::State::CONNECTED &&
      connection_->host_stub()) {
    protocol::PeerConnectionParameters peer_connection_params;
    peer_connection_params.set_preferred_min_bitrate_bps(kMinBitrateBps);
    connection_->host_stub()->ControlPeerConnection(peer_connection_params);
    observers_.Notify(&ClientStatusObserver::OnConnected);
  } else if (state == protocol::ConnectionToHost::State::CLOSED) {
    StopSession();
    observers_.Notify(&ClientStatusObserver::OnDisconnected);
  } else if (state == protocol::ConnectionToHost::State::FAILED) {
    StopSession();
    observers_.Notify(&ClientStatusObserver::OnConnectionFailed);
  }
}

void RemotingClient::OnConnectionReady(bool ready) {
  CLIENT_LOG << "RemotingClient::OnConnectionReady: " << ready;
}

void RemotingClient::OnRouteChanged(const std::string& channel_name,
                                    const protocol::TransportRoute& route) {
  CLIENT_LOG << "Using " << protocol::TransportRoute::GetTypeString(route.type)
             << " connection for " << channel_name << " channel";
}

void RemotingClient::OnSignalStrategyStateChange(SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::CONNECTING:
      CLIENT_LOG << "Signaling channel is being established.";
      break;
    case SignalStrategy::CONNECTED:
      CLIENT_LOG << "Signaling channel has been established for: "
                 << signal_strategy_->GetLocalAddress().id();
      StartConnection();
      break;
    case SignalStrategy::DISCONNECTED:
      auto error = signal_strategy_->GetError();
      auto error_code = ErrorCode::OK;
      if (error != SignalStrategy::Error::OK) {
        LOG(ERROR) << "Signaling channel has been closed due to error: "
                   << error;
        // TODO: joedow - Map error to error_code.
      } else {
        CLIENT_LOG << "Signaling channel has been closed.";
      }
      if (connection_) {
        connection_->Disconnect(error_code);
      }
      RunQuitClosure();
      break;
  }
}

bool RemotingClient::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void RemotingClient::RunQuitClosure() {
  if (quit_closure_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(quit_closure_));
  }
}

}  // namespace remoting
