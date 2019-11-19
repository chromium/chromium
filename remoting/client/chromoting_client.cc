// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include <utility>

#include "remoting/base/capabilities.h"
#include "remoting/base/constants.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_connection_to_host.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/webrtc_connection_to_host.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

ChromotingClient::ChromotingClient(
    ClientContext* client_context,
    ClientUserInterface* user_interface,
    protocol::VideoRenderer* video_renderer,
    base::WeakPtr<protocol::AudioStub> audio_stream_consumer)
    : user_interface_(user_interface), video_renderer_(video_renderer) {
  DCHECK(client_context->main_task_runner()->BelongsToCurrentThread());

  audio_decode_task_runner_ = client_context->audio_decode_task_runner();
  audio_stream_consumer_ = audio_stream_consumer;
}

ChromotingClient::~ChromotingClient() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (signal_strategy_)
      signal_strategy_->RemoveListener(this);
}

void ChromotingClient::set_protocol_config(
    std::unique_ptr<protocol::CandidateSessionConfig> config) {
  DCHECK(!connection_)
      << "set_protocol_config() cannot be called after Start().";
  protocol_config_ = std::move(config);
}

void ChromotingClient::set_host_experiment_config(
    const std::string& experiment_config) {
  DCHECK(!connection_)
      << "set_host_experiment_config() cannot be called after Start().";
  host_experiment_sender_.reset(new HostExperimentSender(experiment_config));
}

void ChromotingClient::SetConnectionToHostForTests(
    std::unique_ptr<protocol::ConnectionToHost> connection_to_host) {
  connection_ = std::move(connection_to_host);
}

void ChromotingClient::Start(
    SignalStrategy* signal_strategy,
    const protocol::ClientAuthenticationConfig& client_auth_config,
    scoped_refptr<protocol::TransportContext> transport_context,
    const std::string& host_jid,
    const std::string& capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!session_manager_);  // Start must not be called more than once.

  host_jid_ = NormalizeSignalingId(host_jid);
  local_capabilities_ = capabilities;

  if (!protocol_config_) {
    protocol_config_ = protocol::CandidateSessionConfig::CreateDefault();
  }

  if (!connection_) {
    if (protocol_config_->webrtc_supported()) {
      DCHECK(!protocol_config_->ice_supported());
#if !defined(ENABLE_WEBRTC_REMOTING_CLIENT)
      LOG(FATAL) << "WebRTC is not supported.";
#else
      connection_.reset(new protocol::WebrtcConnectionToHost());
#endif
    } else {
      DCHECK(protocol_config_->ice_supported());
      connection_.reset(new protocol::IceConnectionToHost());
    }
  }
  connection_->set_client_stub(this);
  connection_->set_clipboard_stub(this);
  connection_->set_video_renderer(video_renderer_);

  if (audio_stream_consumer_) {
    connection_->InitializeAudio(audio_decode_task_runner_,
                                 audio_stream_consumer_);
  } else {
    protocol_config_->DisableAudioChannel();
  }

  session_manager_.reset(new protocol::JingleSessionManager(signal_strategy));
  session_manager_->set_protocol_config(std::move(protocol_config_));

  client_auth_config_ = client_auth_config;
  transport_context_ = transport_context;

  signal_strategy_ = signal_strategy;
  signal_strategy_->AddListener(this);

  switch (signal_strategy_->GetState()) {
    case SignalStrategy::CONNECTING:
      // Nothing to do here. Just need to wait until |signal_strategy_| becomes
      // connected.
      break;
    case SignalStrategy::CONNECTED:
      StartConnection();
      break;
    case SignalStrategy::DISCONNECTED:
      signal_strategy_->Connect();
      break;
  }
}

void ChromotingClient::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connection_->Disconnect(protocol::OK);
}

void ChromotingClient::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Only accept the first |protocol::Capabilities| message.
  if (host_capabilities_received_) {
    LOG(WARNING) << "protocol::Capabilities has been received already.";
    return;
  }

  host_capabilities_received_ = true;
  if (capabilities.has_capabilities())
    host_capabilities_ = capabilities.capabilities();

  VLOG(1) << "Host capabilities: " << host_capabilities_;

  // Calculate the set of capabilities enabled by both client and host and pass
  // it to the webapp.
  user_interface_->SetCapabilities(
      IntersectCapabilities(local_capabilities_, host_capabilities_));
}

void ChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->SetPairingResponse(pairing_response);
}

void ChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->DeliverHostMessage(message);
}

void ChromotingClient::SetVideoLayout(const protocol::VideoLayout& layout) {
  int num_video_tracks = layout.video_track_size();
  if (num_video_tracks < 1) {
    LOG(ERROR) << "Received VideoLayout message with 0 tracks.";
    return;
  }

  if (num_video_tracks > 2) {
    LOG(WARNING) << "Received VideoLayout message with " << num_video_tracks
                 << " tracks. Only one track is supported.";
  }

  const protocol::VideoTrackLayout& track_layout = layout.video_track(0);
  int x_dpi = track_layout.has_x_dpi() ? track_layout.x_dpi() : kDefaultDpi;
  int y_dpi = track_layout.has_y_dpi() ? track_layout.y_dpi() : kDefaultDpi;
  if (x_dpi != y_dpi) {
    LOG(WARNING) << "Mismatched x,y dpi. x=" << x_dpi << " y=" << y_dpi;
  }

  webrtc::DesktopSize size_dips(track_layout.width(), track_layout.height());
  webrtc::DesktopSize size_px(size_dips.width() * x_dpi / kDefaultDpi,
                              size_dips.height() * y_dpi / kDefaultDpi);
  user_interface_->SetDesktopSize(size_px, webrtc::DesktopVector(x_dpi, y_dpi));

  mouse_input_scaler_.set_input_size(size_px.width(), size_px.height());
  if (connection_->config().protocol() ==
      protocol::SessionConfig::Protocol::ICE) {
    mouse_input_scaler_.set_output_size(size_px.width(), size_px.height());
  } else {
    mouse_input_scaler_.set_output_size(size_dips.width(), size_dips.height());
  }
}

void ChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->GetClipboardStub()->InjectClipboardEvent(event);
}

void ChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->GetCursorShapeStub()->SetCursorShape(cursor_shape);
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "ChromotingClient::OnConnectionState(" << state << ")";

  if (state == protocol::ConnectionToHost::CONNECTED) {
    OnChannelsConnected();
  }
  user_interface_->OnConnectionState(state, error);
}

void ChromotingClient::OnConnectionReady(bool ready) {
  VLOG(1) << "ChromotingClient::OnConnectionReady(" << ready << ")";
  user_interface_->OnConnectionReady(ready);
}

void ChromotingClient::OnRouteChanged(const std::string& channel_name,
                                      const protocol::TransportRoute& route) {
  VLOG(0) << "Using " << protocol::TransportRoute::GetTypeString(route.type)
          << " connection for " << channel_name << " channel";
  user_interface_->OnRouteChanged(channel_name, route);
}

void ChromotingClient::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state == SignalStrategy::CONNECTED) {
    VLOG(1) << "Connected as: " << signal_strategy_->GetLocalAddress().id();
    // After signaling has been connected we can try connecting to the host.
    if (connection_ &&
        connection_->state() == protocol::ConnectionToHost::INITIALIZING) {
      StartConnection();
    }
  } else if (state == SignalStrategy::DISCONNECTED) {
    VLOG(1) << "Signaling connection closed.";
    mouse_input_scaler_.set_input_stub(nullptr);
    connection_.reset();
    user_interface_->OnConnectionState(protocol::ConnectionToHost::FAILED,
                                       protocol::SIGNALING_ERROR);
  }
}

bool ChromotingClient::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void ChromotingClient::StartConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto session = session_manager_->Connect(
      SignalingAddress(host_jid_),
      std::make_unique<protocol::NegotiatingClientAuthenticator>(
          signal_strategy_->GetLocalAddress().id(), host_jid_,
          client_auth_config_));
  if (host_experiment_sender_) {
    session->AddPlugin(host_experiment_sender_.get());
  }
  connection_->Connect(std::move(session), transport_context_, this);
}

void ChromotingClient::OnChannelsConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Negotiate capabilities with the host.
  VLOG(1) << "Client capabilities: " << local_capabilities_;

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(local_capabilities_);
  connection_->host_stub()->SetCapabilities(capabilities);

  mouse_input_scaler_.set_input_stub(connection_->input_stub());
}

}  // namespace remoting
