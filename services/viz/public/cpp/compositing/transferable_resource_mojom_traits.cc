// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::TransferableResourceDataView,
                  viz::TransferableResource>::
    Read(viz::mojom::TransferableResourceDataView data,
         viz::TransferableResource* out) {
  CHECK(data.ReadSize(&out->size));
  CHECK(data.ReadMailboxHolder(&out->mailbox_holder));
  CHECK(data.ReadColorSpace(&out->color_space));
  CHECK(data.ReadYcbcrInfo(&out->ycbcr_info));
  out->id = data.id();
  out->format = static_cast<viz::ResourceFormat>(data.format());
  out->filter = data.filter();
  out->read_lock_fences_enabled = data.read_lock_fences_enabled();
  out->is_software = data.is_software();
  out->is_overlay_candidate = data.is_overlay_candidate();
#if defined(OS_ANDROID)
  out->is_backed_by_surface_texture = data.is_backed_by_surface_texture();
  out->wants_promotion_hint = data.wants_promotion_hint();
#endif
  return true;
}

}  // namespace mojo
