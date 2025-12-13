// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/blit_request_mojom_traits.h"

#include <utility>

#include "base/notreached.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/common/exported_shared_image_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "skia/public/mojom/surface_origin_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

// static
viz::mojom::LetterboxingBehavior EnumTraits<
    viz::mojom::LetterboxingBehavior,
    viz::LetterboxingBehavior>::ToMojom(viz::LetterboxingBehavior behavior) {
  switch (behavior) {
    case viz::LetterboxingBehavior::kDoNotLetterbox:
      return viz::mojom::LetterboxingBehavior::kDoNotLetterbox;
    case viz::LetterboxingBehavior::kLetterbox:
      return viz::mojom::LetterboxingBehavior::kLetterbox;
  }
  NOTREACHED();
}

// static
bool EnumTraits<viz::mojom::LetterboxingBehavior, viz::LetterboxingBehavior>::
    FromMojom(viz::mojom::LetterboxingBehavior input,
              viz::LetterboxingBehavior* out) {
  switch (input) {
    case viz::mojom::LetterboxingBehavior::kDoNotLetterbox:
      *out = viz::LetterboxingBehavior::kDoNotLetterbox;
      return true;
    case viz::mojom::LetterboxingBehavior::kLetterbox:
      *out = viz::LetterboxingBehavior::kLetterbox;
      return true;
  }
  return false;
}

gpu::ExportedSharedImage
StructTraits<viz::mojom::BlitRequestDataView, viz::BlitRequest>::shared_image(
    const viz::BlitRequest& blit_request) {
  // Serializing blend_bitmaps is not supported yet. There is no ideal place
  // to have this check in the current code, but this is a good compromise.
  CHECK(blit_request.blend_bitmaps().empty());
  return blit_request.shared_image()->Export();
}

// static
bool StructTraits<viz::mojom::BlitRequestDataView, viz::BlitRequest>::Read(
    viz::mojom::BlitRequestDataView data,
    viz::BlitRequest* out) {
  gfx::Point destination_region_offset;
  if (!data.ReadDestinationRegionOffset(&destination_region_offset)) {
    return false;
  }

  viz::LetterboxingBehavior letterboxing_behavior;
  if (!data.ReadLetterboxingBehavior(&letterboxing_behavior)) {
    return false;
  }

  gpu::ExportedSharedImage exported_shared_image;
  if (!data.ReadSharedImage(&exported_shared_image)) {
    return false;
  }

  gpu::SyncToken sync_token;
  if (!data.ReadSyncToken(&sync_token)) {
    return false;
  }

  *out = viz::BlitRequest(
      destination_region_offset, letterboxing_behavior,
      gpu::ClientSharedImage::ImportUnowned(std::move(exported_shared_image)),
      sync_token, data.populates_gpu_memory_buffer());

  return true;
}

}  // namespace mojo
