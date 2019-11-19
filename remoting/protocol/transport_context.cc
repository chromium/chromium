// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_context.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

#if !defined(OS_NACL)
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/remoting_ice_config_request.h"
#endif  // !defined(OS_NACL)

namespace remoting {
namespace protocol {

namespace {

// Ensure ICE config is correct at least one hour after session starts.
constexpr base::TimeDelta kMinimumIceConfigLifetime =
    base::TimeDelta::FromHours(1);

}  // namespace

#if !defined(OS_NACL)
// static
scoped_refptr<TransportContext> TransportContext::ForTests(TransportRole role) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  return new protocol::TransportContext(
      std::make_unique<protocol::ChromiumPortAllocatorFactory>(), nullptr,
      protocol::NetworkSettings(
          protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING),
      role);
}
#endif  // !defined(OS_NACL)

TransportContext::TransportContext(
    std::unique_ptr<PortAllocatorFactory> port_allocator_factory,
    std::unique_ptr<UrlRequestFactory> url_request_factory,
    const NetworkSettings& network_settings,
    TransportRole role)
    : port_allocator_factory_(std::move(port_allocator_factory)),
      url_request_factory_(std::move(url_request_factory)),
      network_settings_(network_settings),
      role_(role) {}

TransportContext::~TransportContext() = default;

void TransportContext::Prepare() {
  EnsureFreshIceConfig();
}

void TransportContext::GetIceConfig(const GetIceConfigCallback& callback) {
  EnsureFreshIceConfig();

  // If there is a pending |ice_config_request_| for the current |relay_mode_|
  // then delay the callback until the request is finished.
  if (ice_config_request_[relay_mode_]) {
    pending_ice_config_callbacks_[relay_mode_].push_back(callback);
  } else {
    callback.Run(ice_config_[relay_mode_]);
  }
}

void TransportContext::EnsureFreshIceConfig() {
  // Check if request is already pending.
  if (ice_config_request_[relay_mode_])
    return;

  // Don't need to make ICE config request if both STUN and Relay are disabled.
  if ((network_settings_.flags & (NetworkSettings::NAT_TRAVERSAL_STUN |
                                  NetworkSettings::NAT_TRAVERSAL_RELAY)) == 0) {
    return;
  }

  if (ice_config_[relay_mode_].is_null() ||
      base::Time::Now() + kMinimumIceConfigLifetime >
          ice_config_[relay_mode_].expiration_time) {
    std::unique_ptr<IceConfigRequest> request;
    switch (relay_mode_) {
      case RelayMode::TURN:
#if defined(OS_NACL)
        NOTREACHED() << "TURN is not supported on NACL";
#else
        request = std::make_unique<RemotingIceConfigRequest>();
#endif
        break;
    }
    ice_config_request_[relay_mode_] = std::move(request);
    ice_config_request_[relay_mode_]->Send(base::Bind(
        &TransportContext::OnIceConfig, base::Unretained(this), relay_mode_));
  }
}

void TransportContext::OnIceConfig(RelayMode relay_mode,
                                   const IceConfig& ice_config) {
  ice_config_[relay_mode] = ice_config;
  ice_config_request_[relay_mode].reset();

  auto& callback_list = pending_ice_config_callbacks_[relay_mode];
  while (!callback_list.empty()) {
    callback_list.begin()->Run(ice_config);
    callback_list.pop_front();
  }
}

int TransportContext::GetTurnMaxRateKbps() const {
  DCHECK_EQ(relay_mode_, RelayMode::TURN);
  return ice_config_[RelayMode::TURN].max_bitrate_kbps;
}

}  // namespace protocol
}  // namespace remoting
