// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_

#include <optional>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/load_timing_internal_info.h"
#include "net/http/alternate_protocol_usage.h"
#include "services/network/public/mojom/load_timing_internal_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::SessionSource, net::SessionSource> {
  static network::mojom::SessionSource ToMojom(
      net::SessionSource session_source);
  static bool FromMojom(network::mojom::SessionSource in,
                        net::SessionSource* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::AdvertisedAltSvcState,
               net::AdvertisedAltSvcState> {
  static network::mojom::AdvertisedAltSvcState ToMojom(
      net::AdvertisedAltSvcState advertised_alt_svc_state);
  static bool FromMojom(network::mojom::AdvertisedAltSvcState in,
                        net::AdvertisedAltSvcState* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::LoadTimingInternalInfoDataView,
                 net::LoadTimingInternalInfo> {
  static const base::TimeDelta& create_stream_delay(
      const net::LoadTimingInternalInfo& info);
  static const base::TimeDelta& connected_callback_delay(
      const net::LoadTimingInternalInfo& info);
  static const base::TimeDelta& initialize_stream_delay(
      const net::LoadTimingInternalInfo& info);
  static std::optional<net::SessionSource> session_source(
      const net::LoadTimingInternalInfo& info);
  static net::AdvertisedAltSvcState advertised_alt_svc_state(
      const net::LoadTimingInternalInfo& info);
  static bool http_network_session_quic_enabled(
      const net::LoadTimingInternalInfo& info);
  static bool Read(network::mojom::LoadTimingInternalInfoDataView data,
                   net::LoadTimingInternalInfo* info);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INTERNAL_INFO_MOJOM_TRAITS_H_
