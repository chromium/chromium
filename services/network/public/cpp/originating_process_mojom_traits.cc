// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process_mojom_traits.h"

#include "services/network/public/cpp/renderer_process_mojom_traits.h"

namespace mojo {

// static
network::mojom::BrowserProcess UnionTraits<
    network::mojom::OriginatingProcessDataView,
    network::OriginatingProcess>::browser_process(network::OriginatingProcess) {
  return network::mojom::BrowserProcess();
}

// static
bool UnionTraits<network::mojom::OriginatingProcessDataView,
                 network::OriginatingProcess>::
    Read(network::mojom::OriginatingProcessDataView data,
         network::OriginatingProcess* out) {
  switch (data.tag()) {
    case network::mojom::OriginatingProcessDataView::Tag::kBrowserProcess: {
      *out = network::OriginatingProcess::browser();
      return true;
    }
    case network::mojom::OriginatingProcessDataView::Tag::kRendererProcess: {
      network::mojom::RendererProcessDataView view;
      network::RendererProcess renderer_process;
      data.GetRendererProcessDataView(&view);
      if (!StructTraits<decltype(view), decltype(renderer_process)>::Read(
              std::move(view), &renderer_process)) {
        return false;
      }
      *out = network::OriginatingProcess::renderer(std::move(renderer_process));
      return true;
    }
  }
}

}  // namespace mojo
