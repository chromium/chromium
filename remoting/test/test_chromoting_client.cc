// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_chromoting_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio/audio_player.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/third_party_client_authenticator.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/test/connection_setup_info.h"
#include "remoting/test/test_video_renderer.h"
#include "services/network/test/test_shared_url_loader_factory.h"

namespace remoting {
namespace test {

namespace {

// Used as the TokenFetcherCallback for App Remoting sessions.
void FetchThirdPartyToken(
    const std::string& authorization_token,
    const std::string& shared_secret,
    const std::string& token_url,
    const std::string& scope,
    const protocol::ThirdPartyTokenFetchedCallback& token_fetched_callback) {
  VLOG(2) << "FetchThirdPartyToken("
          << "token_url: " << token_url << ", "
          << "scope: " << scope << ") Called";

  token_fetched_callback.Run(authorization_token, shared_secret);
}

void FetchSecret(
    const std::string& client_secret,
    bool pairing_expected,
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  secret_fetched_callback.Run(client_secret);
}

}  // namespace

TestChromotingClient::TestChromotingClient()
    : TestChromotingClient(nullptr) {}

TestChromotingClient::TestChromotingClient(
    std::unique_ptr<protocol::VideoRenderer> video_renderer)
    : connection_to_host_state_(protocol::ConnectionToHost::INITIALIZING),
      connection_error_code_(protocol::OK),
      video_renderer_(std::move(video_renderer)) {}

TestChromotingClient::~TestChromotingClient() {
  // Ensure any connections are closed and the members are destroyed in the
  // appropriate order.
  EndConnection();
}

void TestChromotingClient::StartConnection(
    bool use_test_api_values,
    const ConnectionSetupInfo& connection_setup_info) {
  // Required to establish a connection to the host.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  scoped_refptr<URLRequestContextGetter> request_context_getter;
  request_context_getter =
      new URLRequestContextGetter(base::ThreadTaskRunnerHandle::Get());

  auto test_shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

  client_context_.reset(new ClientContext(base::ThreadTaskRunnerHandle::Get()));

  // Check to see if the user passed in a customized video renderer.
  if (!video_renderer_) {
    video_renderer_.reset(new TestVideoRenderer());
  }

  chromoting_client_.reset(new ChromotingClient(client_context_.get(),
                                                this,  // client_user_interface.
                                                video_renderer_.get(),
                                                nullptr));  // audio_player

  if (test_connection_to_host_) {
    chromoting_client_->SetConnectionToHostForTests(
        std::move(test_connection_to_host_));
  }

  if (!signal_strategy_) {
    // Set up the signal strategy.  This must outlive the client object.
    // TODO(yuweih): This doesn't work since FtlSignalStrategy only works with
    // Tachyon ID. Either integrate with the new directory service or remove
    // the test tool altogether.
    signal_strategy_ = std::make_unique<FtlSignalStrategy>(
        std::make_unique<PassthroughOAuthTokenGetter>(
            connection_setup_info.user_name,
            connection_setup_info.access_token),
        std::make_unique<FtlClientUuidDeviceIdProvider>());
  }

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL);

  scoped_refptr<protocol::TransportContext> transport_context(
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          std::make_unique<ChromiumUrlRequestFactory>(
              test_shared_url_loader_factory),
          network_settings, protocol::TransportRole::CLIENT));

  protocol::ClientAuthenticationConfig client_auth_config;
  client_auth_config.host_id = connection_setup_info.host_id;
  client_auth_config.pairing_client_id = connection_setup_info.pairing_id;
  client_auth_config.pairing_secret = connection_setup_info.shared_secret;

  if (!connection_setup_info.pin.empty()) {
    client_auth_config.fetch_secret_callback =
        base::Bind(&FetchSecret, connection_setup_info.pin);
  }

  client_auth_config.fetch_third_party_token_callback = base::Bind(
      &FetchThirdPartyToken, connection_setup_info.authorization_code,
      connection_setup_info.shared_secret);

  chromoting_client_->Start(signal_strategy_.get(), client_auth_config,
                            transport_context, connection_setup_info.host_jid,
                            connection_setup_info.capabilities);
}

void TestChromotingClient::EndConnection() {
  // Clearing out the client will close the connection.
  chromoting_client_.reset();

  // The signal strategy object must outlive the client so destroy it next.
  signal_strategy_.reset();

  // The connection state will be updated when the chromoting client was
  // destroyed if an active connection was established, but not in other cases.
  // We should be consistent in either case so we will set the state if needed.
  if (connection_to_host_state_ != protocol::ConnectionToHost::CLOSED &&
      connection_to_host_state_ != protocol::ConnectionToHost::FAILED &&
      connection_error_code_ == protocol::OK) {
    OnConnectionState(protocol::ConnectionToHost::CLOSED, protocol::OK);
  }
}

void TestChromotingClient::AddRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.AddObserver(observer);
}

void TestChromotingClient::RemoveRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.RemoveObserver(observer);
}

void TestChromotingClient::SetSignalStrategyForTests(
    std::unique_ptr<SignalStrategy> signal_strategy) {
  signal_strategy_ = std::move(signal_strategy);
}

void TestChromotingClient::SetConnectionToHostForTests(
    std::unique_ptr<protocol::ConnectionToHost> connection_to_host) {
  test_connection_to_host_ = std::move(connection_to_host);
}

void TestChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error_code) {
  VLOG(1) << "TestChromotingClient::OnConnectionState("
          << "state: " << protocol::ConnectionToHost::StateToString(state)
          << ", error_code: " << protocol::ErrorCodeToString(error_code)
          << ") Called";

  connection_error_code_ = error_code;
  connection_to_host_state_ = state;

  for (auto& observer : connection_observers_)
    observer.ConnectionStateChanged(state, error_code);
}

void TestChromotingClient::OnConnectionReady(bool ready) {
  VLOG(1) << "TestChromotingClient::OnConnectionReady("
          << "ready:" << ready << ") Called";

  for (auto& observer : connection_observers_)
    observer.ConnectionReady(ready);
}

void TestChromotingClient::OnRouteChanged(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  VLOG(1) << "TestChromotingClient::OnRouteChanged("
          << "channel_name:" << channel_name << ", "
          << "route:" << protocol::TransportRoute::GetTypeString(route.type)
          << ") Called";

  for (auto& observer : connection_observers_)
    observer.RouteChanged(channel_name, route);
}

void TestChromotingClient::SetCapabilities(const std::string& capabilities) {
  VLOG(1) << "TestChromotingClient::SetCapabilities("
          << "capabilities: " << capabilities << ") Called";

  for (auto& observer : connection_observers_)
    observer.CapabilitiesSet(capabilities);
}

void TestChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  VLOG(1) << "TestChromotingClient::SetPairingResponse("
          << "client_id: " << pairing_response.client_id() << ", "
          << "shared_secret: " << pairing_response.shared_secret()
          << ") Called";

  for (auto& observer : connection_observers_)
    observer.PairingResponseSet(pairing_response);
}

void TestChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  VLOG(1) << "TestChromotingClient::DeliverHostMessage("
          << "type: " << message.type() << ", "
          << "data: " << message.data() << ") Called";

  for (auto& observer : connection_observers_)
    observer.HostMessageReceived(message);
}

void TestChromotingClient::SetDesktopSize(const webrtc::DesktopSize& size,
                      const webrtc::DesktopVector& dpi) {
  VLOG(1) << "TestChromotingClient::SetDesktopSize() Called";
}

protocol::ClipboardStub* TestChromotingClient::GetClipboardStub() {
  VLOG(1) << "TestChromotingClient::GetClipboardStub() Called";
  return this;
}

protocol::CursorShapeStub* TestChromotingClient::GetCursorShapeStub() {
  VLOG(1) << "TestChromotingClient::GetCursorShapeStub() Called";
  return this;
}

void TestChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  VLOG(1) << "TestChromotingClient::InjectClipboardEvent() Called";
}

void TestChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  VLOG(1) << "TestChromotingClient::SetCursorShape() Called";
}

}  // namespace test
}  // namespace remoting
