// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "services/network/public/mojom/net_log.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode> {
  static network::mojom::NetLogCaptureMode ToMojom(
      net::NetLogCaptureMode capture_mode);
  static bool FromMojom(network::mojom::NetLogCaptureMode capture_mode,
                        net::NetLogCaptureMode* out);
};

template <>
struct EnumTraits<network::mojom::NetLogEventPhase, net::NetLogEventPhase> {
  static network::mojom::NetLogEventPhase ToMojom(
      net::NetLogEventPhase capture_mode);
  static bool FromMojom(network::mojom::NetLogEventPhase capture_mode,
                        net::NetLogEventPhase* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_
