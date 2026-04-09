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

network::mojom::ResolutionSource
EnumTraits<network::mojom::ResolutionSource, net::ResolutionSource>::ToMojom(
    net::ResolutionSource resolution_source) {
  switch (resolution_source) {
    case net::ResolutionSource::kUnknown:
      return network::mojom::ResolutionSource::kUnknown;
    case net::ResolutionSource::kCache:
      return network::mojom::ResolutionSource::kCache;
    case net::ResolutionSource::kLocal:
      return network::mojom::ResolutionSource::kLocal;
    case net::ResolutionSource::kInsecure:
      return network::mojom::ResolutionSource::kInsecure;
    case net::ResolutionSource::kSecure:
      return network::mojom::ResolutionSource::kSecure;
    case net::ResolutionSource::kSystem:
      return network::mojom::ResolutionSource::kSystem;
    case net::ResolutionSource::kPlatform:
      return network::mojom::ResolutionSource::kPlatform;
    case net::ResolutionSource::kMdns:
      return network::mojom::ResolutionSource::kMdns;
    case net::ResolutionSource::kNat64:
      return network::mojom::ResolutionSource::kNat64;
  }
  NOTREACHED();
}

net::ResolutionSource
EnumTraits<network::mojom::ResolutionSource, net::ResolutionSource>::FromMojom(
    network::mojom::ResolutionSource in) {
  switch (in) {
    case network::mojom::ResolutionSource::kUnknown:
      return net::ResolutionSource::kUnknown;
    case network::mojom::ResolutionSource::kCache:
      return net::ResolutionSource::kCache;
    case network::mojom::ResolutionSource::kLocal:
      return net::ResolutionSource::kLocal;
    case network::mojom::ResolutionSource::kInsecure:
      return net::ResolutionSource::kInsecure;
    case network::mojom::ResolutionSource::kSecure:
      return net::ResolutionSource::kSecure;
    case network::mojom::ResolutionSource::kSystem:
      return net::ResolutionSource::kSystem;
    case network::mojom::ResolutionSource::kPlatform:
      return net::ResolutionSource::kPlatform;
    case network::mojom::ResolutionSource::kMdns:
      return net::ResolutionSource::kMdns;
    case network::mojom::ResolutionSource::kNat64:
      return net::ResolutionSource::kNat64;
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
bool StructTraits<network::mojom::ResolutionDetailsDataView,
                  net::ResolutionDetails>::
    Read(network::mojom::ResolutionDetailsDataView data,
         net::ResolutionDetails* details) {
  if (!data.ReadSource(&details->source)) {
    return false;
  }
  if (!data.ReadTaskCompletionDelay(&details->task_completion_delay)) {
    return false;
  }
  details->secure_dns_attempted = data.secure_dns_attempted();
  if (!data.ReadDohDetails(&details->doh_details)) {
    return false;
  }
  return true;
}

// static
network::mojom::HttpConnectionInfoCoarse
EnumTraits<network::mojom::HttpConnectionInfoCoarse,
           net::HttpConnectionInfoCoarse>::ToMojom(net::HttpConnectionInfoCoarse
                                                       info) {
  switch (info) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return network::mojom::HttpConnectionInfoCoarse::kHTTP1;
    case net::HttpConnectionInfoCoarse::kHTTP2:
      return network::mojom::HttpConnectionInfoCoarse::kHTTP2;
    case net::HttpConnectionInfoCoarse::kQUIC:
      return network::mojom::HttpConnectionInfoCoarse::kQUIC;
    case net::HttpConnectionInfoCoarse::kOTHER:
      return network::mojom::HttpConnectionInfoCoarse::kOTHER;
  }
  NOTREACHED();
}

// static
net::HttpConnectionInfoCoarse
EnumTraits<network::mojom::HttpConnectionInfoCoarse,
           net::HttpConnectionInfoCoarse>::
    FromMojom(network::mojom::HttpConnectionInfoCoarse in) {
  switch (in) {
    case network::mojom::HttpConnectionInfoCoarse::kHTTP1:
      return net::HttpConnectionInfoCoarse::kHTTP1;
    case network::mojom::HttpConnectionInfoCoarse::kHTTP2:
      return net::HttpConnectionInfoCoarse::kHTTP2;
    case network::mojom::HttpConnectionInfoCoarse::kQUIC:
      return net::HttpConnectionInfoCoarse::kQUIC;
    case network::mojom::HttpConnectionInfoCoarse::kOTHER:
      return net::HttpConnectionInfoCoarse::kOTHER;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::DohResolutionDetailsDataView,
                  net::DohResolutionDetails>::
    Read(network::mojom::DohResolutionDetailsDataView data,
         net::DohResolutionDetails* details) {
  if (!data.ReadSessionSource(&details->session_source)) {
    return false;
  }
  if (!data.ReadConnectionInfo(&details->connection_info)) {
    return false;
  }
  return true;
}

// static
const std::optional<net::ResolutionDetails>&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    resolution_details(const net::LoadTimingInternalInfo& info) {
  return info.resolution_details;
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

  if (!data.ReadResolutionDetails(&info->resolution_details)) {
    return false;
  }
  return true;
}

}  // namespace mojo
