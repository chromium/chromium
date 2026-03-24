// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/base/load_timing_internal_info.h"
#include "net/http/alternate_protocol_usage.h"

namespace mojo {

network::mojom::SessionSource
EnumTraits<network::mojom::SessionSource, net::SessionSource>::ToMojom(
    net::SessionSource session_source) {
  switch (session_source) {
    case net::SessionSource::kNew:
      return network::mojom::SessionSource::kNew;
    case net::SessionSource::kExisting:
      return network::mojom::SessionSource::kExisting;
  }
  NOTREACHED();
}

net::SessionSource
EnumTraits<network::mojom::SessionSource, net::SessionSource>::FromMojom(
    network::mojom::SessionSource in) {
  switch (in) {
    case network::mojom::SessionSource::kNew:
      return net::SessionSource::kNew;
    case network::mojom::SessionSource::kExisting:
      return net::SessionSource::kExisting;
  }
  NOTREACHED();
}

network::mojom::AdvertisedAltSvcState
EnumTraits<network::mojom::AdvertisedAltSvcState, net::AdvertisedAltSvcState>::
    ToMojom(net::AdvertisedAltSvcState session_source) {
  switch (session_source) {
    case net::AdvertisedAltSvcState::kUnknown:
      return network::mojom::AdvertisedAltSvcState::kUnknown;
    case net::AdvertisedAltSvcState::kQuicNotBroken:
      return network::mojom::AdvertisedAltSvcState::kQuicNotBroken;
    case net::AdvertisedAltSvcState::kQuicBroken:
      return network::mojom::AdvertisedAltSvcState::kQuicBroken;
  }
  NOTREACHED();
}

net::AdvertisedAltSvcState
EnumTraits<network::mojom::AdvertisedAltSvcState, net::AdvertisedAltSvcState>::
    FromMojom(network::mojom::AdvertisedAltSvcState in) {
  switch (in) {
    case network::mojom::AdvertisedAltSvcState::kUnknown:
      return net::AdvertisedAltSvcState::kUnknown;
    case network::mojom::AdvertisedAltSvcState::kQuicNotBroken:
      return net::AdvertisedAltSvcState::kQuicNotBroken;
    case network::mojom::AdvertisedAltSvcState::kQuicBroken:
      return net::AdvertisedAltSvcState::kQuicBroken;
  }
  NOTREACHED();
}

// static
std::optional<base::TimeDelta>
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    max_stream_limit_pending_delay(const net::LoadTimingInternalInfo& info) {
  return info.max_stream_limit_pending_delay;
}

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    create_stream_delay(const net::LoadTimingInternalInfo& info) {
  return info.create_stream_delay;
}

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    connected_callback_delay(const net::LoadTimingInternalInfo& info) {
  return info.connected_callback_delay;
}

// static
const base::TimeDelta&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    initialize_stream_delay(const net::LoadTimingInternalInfo& info) {
  return info.initialize_stream_delay;
}

// static
std::optional<net::SessionSource>
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    session_source(const net::LoadTimingInternalInfo& info) {
  return info.session_source;
}

net::AdvertisedAltSvcState
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    advertised_alt_svc_state(const net::LoadTimingInternalInfo& info) {
  return info.advertised_alt_svc_state;
}

bool StructTraits<network::mojom::LoadTimingInternalInfoDataView,
                  net::LoadTimingInternalInfo>::
    http_network_session_quic_enabled(const net::LoadTimingInternalInfo& info) {
  return info.http_network_session_quic_enabled;
}

// static
bool StructTraits<network::mojom::LoadTimingInternalInfoDataView,
                  net::LoadTimingInternalInfo>::
    Read(network::mojom::LoadTimingInternalInfoDataView data,
         net::LoadTimingInternalInfo* info) {
  if (!data.ReadMaxStreamLimitPendingDelay(
          &info->max_stream_limit_pending_delay)) {
    return false;
  }
  if (!data.ReadCreateStreamDelay(&info->create_stream_delay)) {
    return false;
  }
  if (!data.ReadConnectedCallbackDelay(&info->connected_callback_delay)) {
    return false;
  }
  info->accept_ch_frame_received = data.accept_ch_frame_received();
  if (!data.ReadInitializeStreamDelay(&info->initialize_stream_delay)) {
    return false;
  }
  if (!data.ReadSessionSource(&info->session_source)) {
    return false;
  }
  if (!data.ReadAdvertisedAltSvcState(&info->advertised_alt_svc_state)) {
    return false;
  }
  info->http_network_session_quic_enabled =
      data.http_network_session_quic_enabled();
  return true;
}

}  // namespace mojo
