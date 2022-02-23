// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include "build/build_config.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::TransferableResourceDataView,
                  viz::TransferableResource>::
    Read(viz::mojom::TransferableResourceDataView data,
         viz::TransferableResource* out) {
  viz::ResourceId id;
  if (!data.ReadSize(&out->size) || !data.ReadFormat(&out->format) ||
      !data.ReadMailboxHolder(&out->mailbox_holder) ||
      !data.ReadColorSpace(&out->color_space) ||
      !data.ReadHdrMetadata(&out->hdr_metadata) ||
      !data.ReadYcbcrInfo(&out->ycbcr_info) || !data.ReadId(&id)) {
    return false;
  }
  out->id = id;
  out->filter = data.filter();
  out->read_lock_fences_enabled = data.read_lock_fences_enabled();
  out->is_software = data.is_software();
  out->is_overlay_candidate = data.is_overlay_candidate();

#if BUILDFLAG(IS_ANDROID)
  out->is_backed_by_surface_texture = data.is_backed_by_surface_texture();
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  out->wants_promotion_hint = data.wants_promotion_hint();
#endif

  return true;
}

}  // namespace mojo
