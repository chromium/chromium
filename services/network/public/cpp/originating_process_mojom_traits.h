// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_MOJOM_TRAITS_H_

#include "services/network/public/cpp/originating_process.h"
#include "services/network/public/mojom/originating_process.mojom.h"

namespace mojo {

template <>
struct UnionTraits<network::mojom::OriginatingProcessDataView,
                   network::OriginatingProcess> {
  static network::mojom::OriginatingProcessDataView::Tag GetTag(
      const network::OriginatingProcess& process) {
    if (process.is_browser()) {
      return network::mojom::OriginatingProcessDataView::Tag::kBrowserProcess;
    }
    return network::mojom::OriginatingProcessDataView::Tag::kRendererProcess;
  }

  static network::mojom::BrowserProcess browser_process(
      network::OriginatingProcess);

  static network::RendererProcess renderer_process(
      network::OriginatingProcess process) {
    return process.renderer_process();
  }

  static bool Read(network::mojom::OriginatingProcessDataView data,
                   network::OriginatingProcess* out);
};

template <>
struct StructTraits<network::mojom::BrowserProcessDataView,
                    network::mojom::BrowserProcess> {
  static bool Read(network::mojom::BrowserProcessDataView data,
                   network::mojom::BrowserProcess* out) {
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_MOJOM_TRAITS_H_
