// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "services/network/public/mojom/net_log.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode> {
  static network::mojom::NetLogCaptureMode ToMojom(
      net::NetLogCaptureMode capture_mode);
  static bool FromMojom(network::mojom::NetLogCaptureMode capture_mode,
                        net::NetLogCaptureMode* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NET_LOG_MOJOM_TRAITS_H_
