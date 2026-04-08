// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_

#include <optional>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/load_timing_internal_info.h"
#include "net/dns/public/resolution_details.h"
#include "net/http/alternate_protocol_usage.h"
#include "services/network/public/mojom/load_timing_internal_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::SessionSource, net::SessionSource> {
  static network::mojom::SessionSource ToMojom(
      net::SessionSource session_source);
  static net::SessionSource FromMojom(network::mojom::SessionSource in);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::AdvertisedAltSvcState,
               net::AdvertisedAltSvcState> {
  static network::mojom::AdvertisedAltSvcState ToMojom(
      net::AdvertisedAltSvcState advertised_alt_svc_state);
  static net::AdvertisedAltSvcState FromMojom(
      network::mojom::AdvertisedAltSvcState in);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::ResolutionSource, net::ResolutionSource> {
  static network::mojom::ResolutionSource ToMojom(
      net::ResolutionSource resolution_source);
  static net::ResolutionSource FromMojom(network::mojom::ResolutionSource in);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::ResolutionDetailsDataView,
                 net::ResolutionDetails> {
  static net::ResolutionSource source(const net::ResolutionDetails& details) {
    return details.source;
  }
  static std::optional<base::TimeDelta> task_completion_delay(
      const net::ResolutionDetails& details) {
    return details.task_completion_delay;
  }
  static bool secure_dns_attempted(const net::ResolutionDetails& details) {
    return details.secure_dns_attempted;
  }
  static bool Read(network::mojom::ResolutionDetailsDataView data,
                   net::ResolutionDetails* details);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::LoadTimingInternalInfoDataView,
                 net::LoadTimingInternalInfo> {
  static std::optional<base::TimeDelta> max_stream_limit_pending_delay(
      const net::LoadTimingInternalInfo& info);
  static const base::TimeDelta& create_stream_delay(
      const net::LoadTimingInternalInfo& info);
  static const base::TimeDelta& connected_callback_delay(
      const net::LoadTimingInternalInfo& info);
  static bool accept_ch_frame_received(
      const net::LoadTimingInternalInfo& info) {
    return info.accept_ch_frame_received;
  }
  static const base::TimeDelta& initialize_stream_delay(
      const net::LoadTimingInternalInfo& info);
  static std::optional<net::SessionSource> session_source(
      const net::LoadTimingInternalInfo& info);
  static net::AdvertisedAltSvcState advertised_alt_svc_state(
      const net::LoadTimingInternalInfo& info);
  static bool http_network_session_quic_enabled(
      const net::LoadTimingInternalInfo& info);
  static const std::optional<net::ResolutionDetails>& resolution_details(
      const net::LoadTimingInternalInfo& info);
  static bool Read(network::mojom::LoadTimingInternalInfoDataView data,
                   net::LoadTimingInternalInfo* info);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_
