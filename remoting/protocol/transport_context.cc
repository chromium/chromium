// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_context.h"

#include <sstream>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/webrtc/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/remoting_ice_config_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting::protocol {

namespace {

// Use a cooldown period to prevent multiple service requests in case of a bug.
constexpr base::TimeDelta kIceConfigRequestCooldown = base::Minutes(2);

void PrintIceConfig(const IceConfig& ice_config) {
  std::stringstream ss;
  ss << "\nIceConfig: {\n";
  ss << "  stun: [\n";
  for (auto& stun_server : ice_config.stun_servers) {
    ss << "    " << stun_server.ToString() << ",\n";
  }
  ss << "  ]\n";
  ss << "  turn: [\n";
  for (auto& turn_server : ice_config.turn_servers) {
    ss << "    {\n";
    ss << "      username: " << turn_server.credentials.username << "\n";
    ss << "      password: " << turn_server.credentials.password << "\n";
    for (auto& port : turn_server.ports) {
      ss << "      port: " << port.address.ToString() << "\n";
    }
    ss << "    },\n";
  }
  ss << "  ]\n";
  ss << "  expiration time: " << ice_config.expiration_time << "\n";
  ss << "  max_bitrate_kbps: " << ice_config.max_bitrate_kbps << "\n";
  ss << "}";
  HOST_LOG << ss.str();
}

}  // namespace

// static
scoped_refptr<TransportContext> TransportContext::ForTests(TransportRole role) {
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  return new protocol::TransportContext(
      std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
      webrtc::ThreadWrapper::current()->SocketServer(), nullptr, nullptr,
      protocol::NetworkSettings(
          protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING),
      role);
}

TransportContext::TransportContext(
    std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
    rtc::SocketFactory* socket_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuthTokenGetter* oauth_token_getter,
    const NetworkSettings& network_settings,
    TransportRole role)
    : port_allocator_factory_(std::move(port_allocator_factory)),
      socket_factory_(socket_factory),
      url_loader_factory_(url_loader_factory),
      oauth_token_getter_(oauth_token_getter),
      network_settings_(network_settings),
      role_(role) {
  DCHECK(socket_factory_);
}

TransportContext::~TransportContext() = default;

void TransportContext::Prepare() {
  EnsureFreshIceConfig();
}

void TransportContext::GetIceConfig(GetIceConfigCallback callback) {
  EnsureFreshIceConfig();

  // If there is a pending |ice_config_request_| then delay the callback until
  // the request is finished.
  if (ice_config_request_) {
    pending_ice_config_callbacks_.push_back(std::move(callback));
  } else {
    HOST_LOG << "Using cached ICE Config.";
    PrintIceConfig(ice_config_);
    std::move(callback).Run(ice_config_);
  }
}

void TransportContext::EnsureFreshIceConfig() {
  // Check if request is already pending.
  if (ice_config_request_) {
    HOST_LOG << "ICE Config request is already pending.";
    return;
  }

  // Don't need to make ICE config request if both STUN and Relay are disabled.
  if ((network_settings_.flags & (NetworkSettings::NAT_TRAVERSAL_STUN |
                                  NetworkSettings::NAT_TRAVERSAL_RELAY)) == 0) {
    HOST_LOG << "Skipping ICE Config request as STUN and RELAY are disabled";
    return;
  }

  if (last_request_completion_time_.is_max()) {
    HOST_LOG << "Skipping ICE Config request as refreshing is disabled";
    return;
  }

  if (base::Time::Now() >
      (last_request_completion_time_ + kIceConfigRequestCooldown)) {
    ice_config_request_ = std::make_unique<RemotingIceConfigRequest>(
        url_loader_factory_, oauth_token_getter_);
    ice_config_request_->Send(
        base::BindOnce(&TransportContext::OnIceConfig, base::Unretained(this)));
  } else {
    HOST_LOG << "Skipping ICE Config request made during the cooldown period.";
  }
}

void TransportContext::OnIceConfig(const IceConfig& ice_config) {
  ice_config_ = ice_config;
  ice_config_request_.reset();

  if (!ice_config_.is_null()) {
    // Only reset |last_request_completion_time_| if we received a valid config.
    // If we received an empty config, it could mean a problem in the backend,
    // a network issue, or some other error. Regardless of the specific error,
    // we should try to fetch a new config the next time one is requested.
    last_request_completion_time_ = base::Time::Now();
  }

  HOST_LOG << "Using newly requested ICE Config.";
  PrintIceConfig(ice_config);

  auto& callback_list = pending_ice_config_callbacks_;
  while (!callback_list.empty()) {
    std::move(callback_list.front()).Run(ice_config);
    callback_list.pop_front();
  }
}

int TransportContext::GetTurnMaxRateKbps() const {
  return ice_config_.max_bitrate_kbps;
}

}  // namespace remoting::protocol
