// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/remoting_client.h"

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
#include "remoting/client/common/logging.h"
#include "remoting/proto/remoting/v1/remote_support_host_messages.pb.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/ice_config_fetcher_default.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

RemotingClient::RemotingClient(
    base::OnceClosure quit_closure,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : quit_closure_(std::move(quit_closure)),
      url_loader_factory_(url_loader_factory) {}

RemotingClient::~RemotingClient() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void RemotingClient::StartSession(std::string_view support_id,
                                  OAuthTokenInfo oauth_token_info) {
  CHECK(!session_manager_);
  CHECK_EQ(support_id.length(), 12);
  CHECK_GT(oauth_token_info.access_token().length(), 0);
  CHECK_GT(oauth_token_info.user_email().length(), 0);

  chrome_os_host_id_ = support_id;

  // TODO: joedow - If we need to support sessions > 1 hour, we will need to
  // provide a method for refreshing the access token.
  oauth_token_info_ = std::move(oauth_token_info);
  oauth_token_getter_ =
      std::make_unique<PassthroughOAuthTokenGetter>(oauth_token_info_);
  directory_service_client_ = std::make_unique<DirectoryServiceClient>(
      oauth_token_getter_.get(), url_loader_factory_);

  // base::Unretained is sound because this instance owns the service client
  // and callbacks will not be run after destruction.
  CLIENT_LOG << "Retrieving host information for id: " << chrome_os_host_id_;
  directory_service_client_->GetManagedChromeOsHost(
      chrome_os_host_id_.substr(0, 7),
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
    std::move(quit_closure_).Run();
    return;
  }

  CLIENT_LOG << "Initializing signaling...";
  signal_strategy_ = std::make_unique<FtlSignalStrategy>(
      std::make_unique<PassthroughOAuthTokenGetter>(oauth_token_info_),
      url_loader_factory_, std::make_unique<FtlClientUuidDeviceIdProvider>());
  signal_strategy_->AddListener(this);
  signal_strategy_->Connect();

  CLIENT_LOG << "Creating transport context...";
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          webrtc::ThreadWrapper::current()->SocketServer(),
          std::make_unique<protocol::IceConfigFetcherDefault>(
              url_loader_factory_, oauth_token_getter_.get()),
          protocol::TransportRole::CLIENT);

  CLIENT_LOG << "Creating session manager...";
  auto protocol_config = protocol::CandidateSessionConfig::CreateDefault();
  protocol_config->DisableAudioChannel();
  protocol_config->set_webrtc_supported(true);
  session_manager_ =
      std::make_unique<protocol::JingleSessionManager>(signal_strategy_.get());
  session_manager_->set_protocol_config(std::move(protocol_config));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SignalStrategy::Disconnect,
                     base::Unretained(signal_strategy_.get())),
      base::Seconds(5));
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
  NOTIMPLEMENTED();
}

void RemotingClient::OnConnectionReady(bool ready) {
  NOTIMPLEMENTED();
}

void RemotingClient::OnRouteChanged(const std::string& channel_name,
                                    const protocol::TransportRoute& route) {
  NOTIMPLEMENTED();
}

void RemotingClient::OnSignalStrategyStateChange(SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::CONNECTING:
      CLIENT_LOG << "Signaling channel is being established.";
      break;
    case SignalStrategy::CONNECTED:
      CLIENT_LOG << "Signaling channel has been established for: "
                 << signal_strategy_->GetLocalAddress().id();
      // TODO: joedow - Start the connection process.
      break;
    case SignalStrategy::DISCONNECTED:
      auto error = signal_strategy_->GetError();
      if (error != SignalStrategy::Error::OK) {
        LOG(ERROR) << "Signaling channel has been closed due to error: "
                   << error;
      } else {
        CLIENT_LOG << "Signaling channel has been closed.";
      }

      RunQuitClosure();
      break;
  }
}

bool RemotingClient::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  CLIENT_LOG << "OnSignalStrategyIncomingStanza";
  NOTIMPLEMENTED();
  return false;
}

void RemotingClient::RunQuitClosure() {
  if (quit_closure_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(quit_closure_));
  }
}

}  // namespace remoting
