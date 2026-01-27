// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_MOJOM_TRAITS_H_

#include "services/network/public/cpp/renderer_process.h"
#include "services/network/public/mojom/originating_process.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::RendererProcessDataView,
                    network::RendererProcess> {
  static int32_t process_id(network::RendererProcess id) { return id.value(); }

  static bool Read(network::mojom::RendererProcessDataView data,
                   network::RendererProcess* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RENDERER_PROCESS_MOJOM_TRAITS_H_
