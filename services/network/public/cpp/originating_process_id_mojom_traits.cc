// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process_id_mojom_traits.h"

#include "services/network/public/cpp/renderer_process_id_mojom_traits.h"

namespace mojo {

// static
network::mojom::BrowserProcessId
UnionTraits<network::mojom::OriginatingProcessIdDataView,
            network::OriginatingProcessId>::
    browser_process_id(network::OriginatingProcessId) {
  return network::mojom::BrowserProcessId();
}

// static
bool UnionTraits<network::mojom::OriginatingProcessIdDataView,
                 network::OriginatingProcessId>::
    Read(network::mojom::OriginatingProcessIdDataView data,
         network::OriginatingProcessId* out) {
  switch (data.tag()) {
    case network::mojom::OriginatingProcessIdDataView::Tag::kBrowserProcessId: {
      network::mojom::BrowserProcessId browser_process_id;
      if (!data.ReadBrowserProcessId(&browser_process_id)) {
        return false;
      }
      *out = network::OriginatingProcessId::browser();
      return true;
    }
    case network::mojom::OriginatingProcessIdDataView::Tag::
        kRendererProcessId: {
      network::RendererProcessId renderer_process_id;
      if (!data.ReadRendererProcessId(&renderer_process_id)) {
        return false;
      }
      *out = network::OriginatingProcessId::renderer(
          std::move(renderer_process_id));
      return true;
    }
  }
}

}  // namespace mojo
