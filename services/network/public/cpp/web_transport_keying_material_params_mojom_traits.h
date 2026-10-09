// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_KEYING_MATERIAL_PARAMS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_KEYING_MATERIAL_PARAMS_MOJOM_TRAITS_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/mojom/web_transport.mojom-shared.h"

namespace network {

struct COMPONENT_EXPORT(NETWORK_CPP_BASE) WebTransportKeyingMaterialParams {
  std::vector<uint8_t> label;
  std::vector<uint8_t> context;
  uint32_t output_length = 0;
};

}  // namespace network

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::WebTransportKeyingMaterialParamsDataView,
                 network::WebTransportKeyingMaterialParams> {
  static const std::vector<uint8_t>& label(
      const network::WebTransportKeyingMaterialParams& input) {
    return input.label;
  }

  static const std::vector<uint8_t>& context(
      const network::WebTransportKeyingMaterialParams& input) {
    return input.context;
  }

  static uint32_t output_length(
      const network::WebTransportKeyingMaterialParams& input) {
    return input.output_length;
  }

  static bool Read(
      network::mojom::WebTransportKeyingMaterialParamsDataView data,
      network::WebTransportKeyingMaterialParams* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_KEYING_MATERIAL_PARAMS_MOJOM_TRAITS_H_
