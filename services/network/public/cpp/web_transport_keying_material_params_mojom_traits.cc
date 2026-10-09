// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_keying_material_params_mojom_traits.h"

#include "services/network/public/mojom/web_transport.mojom.h"

namespace mojo {

// static
bool StructTraits<network::mojom::WebTransportKeyingMaterialParamsDataView,
                  network::WebTransportKeyingMaterialParams>::
    Read(network::mojom::WebTransportKeyingMaterialParamsDataView data,
         network::WebTransportKeyingMaterialParams* out) {
  if (!data.ReadLabel(&out->label) || !data.ReadContext(&out->context)) {
    return false;
  }

  out->output_length = data.output_length();
  return out->label.size() <=
             network::mojom::kWebTransportExporterMaxInputLength &&
         out->context.size() <=
             network::mojom::kWebTransportExporterMaxInputLength &&
         out->output_length > 0 &&
         out->output_length <=
             network::mojom::kWebTransportExporterMaxOutputLength;
}

}  // namespace mojo
