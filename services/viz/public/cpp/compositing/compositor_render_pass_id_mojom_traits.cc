// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"

#include "components/viz/common/quads/compositor_render_pass.h"

namespace mojo {

// static
uint64_t StructTraits<
    viz::mojom::CompositorRenderPassIdDataView,
    viz::CompositorRenderPassId>::value(const viz::CompositorRenderPassId& id) {
  return static_cast<uint64_t>(id);
}

// static
bool StructTraits<viz::mojom::CompositorRenderPassIdDataView,
                  viz::CompositorRenderPassId>::
    Read(viz::mojom::CompositorRenderPassIdDataView data,
         viz::CompositorRenderPassId* out) {
  *out = viz::CompositorRenderPassId{data.value()};
  return true;
}

}  // namespace mojo
