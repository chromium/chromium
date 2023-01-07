// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ALTERNATE_PROTOCOL_USAGE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ALTERNATE_PROTOCOL_USAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/http/alternate_protocol_usage.h"
#include "services/network/public/mojom/alternate_protocol_usage.mojom-shared.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_IP_ADDRESS)
    EnumTraits<network::mojom::AlternateProtocolUsage,
               net::AlternateProtocolUsage> {
  static network::mojom::AlternateProtocolUsage ToMojom(
      net::AlternateProtocolUsage input);
  static bool FromMojom(network::mojom::AlternateProtocolUsage input,
                        net::AlternateProtocolUsage* output);
};
}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ALTERNATE_PROTOCOL_USAGE_MOJOM_TRAITS_H_