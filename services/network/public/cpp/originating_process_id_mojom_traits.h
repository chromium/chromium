// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_MOJOM_TRAITS_H_

#include "services/network/public/cpp/originating_process_id.h"
#include "services/network/public/mojom/originating_process_id.mojom.h"

namespace mojo {

template <>
struct UnionTraits<network::mojom::OriginatingProcessIdDataView,
                   network::OriginatingProcessId> {
  static network::mojom::OriginatingProcessIdDataView::Tag GetTag(
      const network::OriginatingProcessId& process) {
    if (process.is_browser()) {
      return network::mojom::OriginatingProcessIdDataView::Tag::
          kBrowserProcessId;
    }
    return network::mojom::OriginatingProcessIdDataView::Tag::
        kRendererProcessId;
  }

  static network::mojom::BrowserProcessId browser_process_id(
      network::OriginatingProcessId);

  static network::RendererProcessId renderer_process_id(
      network::OriginatingProcessId process) {
    return process.renderer_process_id();
  }

  static bool Read(network::mojom::OriginatingProcessIdDataView data,
                   network::OriginatingProcessId* out);
};

template <>
struct StructTraits<network::mojom::BrowserProcessIdDataView,
                    network::mojom::BrowserProcessId> {
  static bool Read(network::mojom::BrowserProcessIdDataView data,
                   network::mojom::BrowserProcessId* out) {
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_MOJOM_TRAITS_H_
