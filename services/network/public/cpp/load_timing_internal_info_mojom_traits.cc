// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/base/load_timing_internal_info.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"

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

network::mojom::MultiplexedSessionCreationInitiator
EnumTraits<network::mojom::MultiplexedSessionCreationInitiator,
           net::MultiplexedSessionCreationInitiator>::
    ToMojom(net::MultiplexedSessionCreationInitiator initiator) {
  switch (initiator) {
    case net::MultiplexedSessionCreationInitiator::kUnknown:
      return network::mojom::MultiplexedSessionCreationInitiator::kUnknown;
    case net::MultiplexedSessionCreationInitiator::kPreconnect:
      return network::mojom::MultiplexedSessionCreationInitiator::kPreconnect;
  }
  NOTREACHED();
}

net::MultiplexedSessionCreationInitiator
EnumTraits<network::mojom::MultiplexedSessionCreationInitiator,
           net::MultiplexedSessionCreationInitiator>::
    FromMojom(network::mojom::MultiplexedSessionCreationInitiator in) {
  switch (in) {
    case network::mojom::MultiplexedSessionCreationInitiator::kUnknown:
      return net::MultiplexedSessionCreationInitiator::kUnknown;
    case network::mojom::MultiplexedSessionCreationInitiator::kPreconnect:
      return net::MultiplexedSessionCreationInitiator::kPreconnect;
  }
  NOTREACHED();
}

network::mojom::QuicSessionEstablishmentReason
EnumTraits<network::mojom::QuicSessionEstablishmentReason,
           net::QuicSessionEstablishmentReason>::
    ToMojom(net::QuicSessionEstablishmentReason reason) {
  switch (reason) {
    case net::QuicSessionEstablishmentReason::kUnknown:
      return network::mojom::QuicSessionEstablishmentReason::kUnknown;
    case net::QuicSessionEstablishmentReason::kNoSessionExisted:
      return network::mojom::QuicSessionEstablishmentReason::kNoSessionExisted;
    case net::QuicSessionEstablishmentReason::kSessionExistedButNotPreconnect:
      return network::mojom::QuicSessionEstablishmentReason::
          kSessionExistedButNotPreconnect;
    case net::QuicSessionEstablishmentReason::kSessionExistedAndWasPreconnect:
      return network::mojom::QuicSessionEstablishmentReason::
          kSessionExistedAndWasPreconnect;
    case net::QuicSessionEstablishmentReason::kSessionExistedBoth:
      return network::mojom::QuicSessionEstablishmentReason::
          kSessionExistedBoth;
    case net::QuicSessionEstablishmentReason::kInflightSessionButNotPreconnect:
      return network::mojom::QuicSessionEstablishmentReason::
          kInflightSessionButNotPreconnect;
    case net::QuicSessionEstablishmentReason::kInflightSessionAndWasPreconnect:
      return network::mojom::QuicSessionEstablishmentReason::
          kInflightSessionAndWasPreconnect;
  }
  NOTREACHED();
}

net::QuicSessionEstablishmentReason
EnumTraits<network::mojom::QuicSessionEstablishmentReason,
           net::QuicSessionEstablishmentReason>::
    FromMojom(network::mojom::QuicSessionEstablishmentReason in) {
  switch (in) {
    case network::mojom::QuicSessionEstablishmentReason::kUnknown:
      return net::QuicSessionEstablishmentReason::kUnknown;
    case network::mojom::QuicSessionEstablishmentReason::kNoSessionExisted:
      return net::QuicSessionEstablishmentReason::kNoSessionExisted;
    case network::mojom::QuicSessionEstablishmentReason::
        kSessionExistedButNotPreconnect:
      return net::QuicSessionEstablishmentReason::
          kSessionExistedButNotPreconnect;
    case network::mojom::QuicSessionEstablishmentReason::
        kSessionExistedAndWasPreconnect:
      return net::QuicSessionEstablishmentReason::
          kSessionExistedAndWasPreconnect;
    case network::mojom::QuicSessionEstablishmentReason::kSessionExistedBoth:
      return net::QuicSessionEstablishmentReason::kSessionExistedBoth;
    case network::mojom::QuicSessionEstablishmentReason::
        kInflightSessionButNotPreconnect:
      return net::QuicSessionEstablishmentReason::
          kInflightSessionButNotPreconnect;
    case network::mojom::QuicSessionEstablishmentReason::
        kInflightSessionAndWasPreconnect:
      return net::QuicSessionEstablishmentReason::
          kInflightSessionAndWasPreconnect;
  }
  NOTREACHED();
}

network::mojom::QuicSessionNonReuseReason EnumTraits<
    network::mojom::QuicSessionNonReuseReason,
    net::QuicSessionNonReuseReason>::ToMojom(net::QuicSessionNonReuseReason
                                                 reason) {
  switch (reason) {
    case net::QuicSessionNonReuseReason::kNoSessionExisted_TrueColdStart:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_TrueColdStart;
    case net::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_SocketTag:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_SocketTag;
    case net::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_NetworkAnonymizationKey:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_NetworkAnonymizationKey;
    case net::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_PrivacyMode:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_PrivacyMode;
    case net::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_SecureDnsPolicy:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_SecureDnsPolicy;
    case net::QuicSessionNonReuseReason::kNoSessionExisted_KeyMismatch_Other:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_Other;
    case net::QuicSessionNonReuseReason::kSessionExisted_ServerGoaway:
      return network::mojom::QuicSessionNonReuseReason::
          kSessionExisted_ServerGoaway;
    case net::QuicSessionNonReuseReason::kSessionExisted_Disconnected:
      return network::mojom::QuicSessionNonReuseReason::
          kSessionExisted_Disconnected;
    case net::QuicSessionNonReuseReason::kSessionExisted_OtherGoingAway:
      return network::mojom::QuicSessionNonReuseReason::
          kSessionExisted_OtherGoingAway;
    case net::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_MultipleFields:
      return network::mojom::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_MultipleFields;
    case net::QuicSessionNonReuseReason::kSessionExisted_MultipleReasons:
      return network::mojom::QuicSessionNonReuseReason::
          kSessionExisted_MultipleReasons;
  }
  NOTREACHED();
}

net::QuicSessionNonReuseReason
EnumTraits<network::mojom::QuicSessionNonReuseReason,
           net::QuicSessionNonReuseReason>::
    FromMojom(network::mojom::QuicSessionNonReuseReason in) {
  switch (in) {
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_TrueColdStart:
      return net::QuicSessionNonReuseReason::kNoSessionExisted_TrueColdStart;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_SocketTag:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_SocketTag;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_NetworkAnonymizationKey:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_NetworkAnonymizationKey;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_PrivacyMode:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_PrivacyMode;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_SecureDnsPolicy:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_SecureDnsPolicy;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_Other:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_Other;
    case network::mojom::QuicSessionNonReuseReason::
        kSessionExisted_ServerGoaway:
      return net::QuicSessionNonReuseReason::kSessionExisted_ServerGoaway;
    case network::mojom::QuicSessionNonReuseReason::
        kSessionExisted_Disconnected:
      return net::QuicSessionNonReuseReason::kSessionExisted_Disconnected;
    case network::mojom::QuicSessionNonReuseReason::
        kSessionExisted_OtherGoingAway:
      return net::QuicSessionNonReuseReason::kSessionExisted_OtherGoingAway;
    case network::mojom::QuicSessionNonReuseReason::
        kNoSessionExisted_KeyMismatch_MultipleFields:
      return net::QuicSessionNonReuseReason::
          kNoSessionExisted_KeyMismatch_MultipleFields;
    case network::mojom::QuicSessionNonReuseReason::
        kSessionExisted_MultipleReasons:
      return net::QuicSessionNonReuseReason::kSessionExisted_MultipleReasons;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::QuicConnectionReuseDetailsDataView,
                  net::QuicConnectionReuseDetails>::
    Read(network::mojom::QuicConnectionReuseDetailsDataView data,
         net::QuicConnectionReuseDetails* details) {
  if (!data.ReadEstablishmentReason(&details->establishment_reason)) {
    return false;
  }
  if (!data.ReadNonReuseReason(&details->non_reuse_reason)) {
    return false;
  }
  return true;
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
const std::optional<net::QuicConnectionReuseDetails>&
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    quic_connection_reuse_details(const net::LoadTimingInternalInfo& info) {
  return info.quic_connection_reuse_details;
}

// static
std::optional<net::MultiplexedSessionCreationInitiator>
StructTraits<network::mojom::LoadTimingInternalInfoDataView,
             net::LoadTimingInternalInfo>::
    session_creation_initiator(const net::LoadTimingInternalInfo& info) {
  return info.session_creation_initiator;
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
network::mojom::NextProto
EnumTraits<network::mojom::NextProto, net::NextProto>::ToMojom(
    net::NextProto next_proto) {
  switch (next_proto) {
    case net::NextProto::kProtoUnknown:
      return network::mojom::NextProto::kProtoUnknown;
    case net::NextProto::kProtoHTTP11:
      return network::mojom::NextProto::kProtoHTTP11;
    case net::NextProto::kProtoHTTP2:
      return network::mojom::NextProto::kProtoHTTP2;
    case net::NextProto::kProtoQUIC:
      return network::mojom::NextProto::kProtoQUIC;
  }
  NOTREACHED();
}

// static
net::NextProto EnumTraits<network::mojom::NextProto, net::NextProto>::FromMojom(
    network::mojom::NextProto in) {
  switch (in) {
    case network::mojom::NextProto::kProtoUnknown:
      return net::NextProto::kProtoUnknown;
    case network::mojom::NextProto::kProtoHTTP11:
      return net::NextProto::kProtoHTTP11;
    case network::mojom::NextProto::kProtoHTTP2:
      return net::NextProto::kProtoHTTP2;
    case network::mojom::NextProto::kProtoQUIC:
      return net::NextProto::kProtoQUIC;
  }
  NOTREACHED();
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
  if (!data.ReadQuicConnectionReuseDetails(
          &info->quic_connection_reuse_details)) {
    return false;
  }
  if (!data.ReadSessionCreationInitiator(&info->session_creation_initiator)) {
    return false;
  }
  return true;
}

}  // namespace mojo
