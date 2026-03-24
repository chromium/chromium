// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include <utility>

#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/ipc/common/exported_shared_image_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::MetadataOverrideDataView,
                  viz::TransferableResource::MetadataOverride>::
    Read(viz::mojom::MetadataOverrideDataView data,
         viz::TransferableResource::MetadataOverride* out) {
  out->is_overlay_candidate = data.is_overlay_candidate();
  if (!data.ReadColorSpace(&out->color_space) ||
      !data.ReadOrigin(&out->origin) || !data.ReadAlphaType(&out->alpha_type)) {
    return false;
  }
  return true;
}

// static
viz::mojom::SynchronizationType
EnumTraits<viz::mojom::SynchronizationType,
           viz::TransferableResource::SynchronizationType>::
    ToMojom(viz::TransferableResource::SynchronizationType type) {
  switch (type) {
    case viz::TransferableResource::SynchronizationType::kSyncToken:
      return viz::mojom::SynchronizationType::kSyncToken;
    case viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted:
      return viz::mojom::SynchronizationType::kGpuCommandsCompleted;
    case viz::TransferableResource::SynchronizationType::kReleaseFence:
      return viz::mojom::SynchronizationType::kReleaseFence;
  }
  NOTREACHED();
}

// static
viz::TransferableResource::SynchronizationType
EnumTraits<viz::mojom::SynchronizationType,
           viz::TransferableResource::SynchronizationType>::
    FromMojom(viz::mojom::SynchronizationType input) {
  switch (input) {
    case viz::mojom::SynchronizationType::kSyncToken:
      return viz::TransferableResource::SynchronizationType::kSyncToken;
    case viz::mojom::SynchronizationType::kGpuCommandsCompleted:
      return viz::TransferableResource::SynchronizationType::
          kGpuCommandsCompleted;
    case viz::mojom::SynchronizationType::kReleaseFence:
      return viz::TransferableResource::SynchronizationType::kReleaseFence;
  }
  NOTREACHED();
}

// static
viz::mojom::ResourceSource
EnumTraits<viz::mojom::ResourceSource,
           viz::TransferableResource::ResourceSource>::
    ToMojom(viz::TransferableResource::ResourceSource type) {
  switch (type) {
    case viz::TransferableResource::ResourceSource::kUnknown:
      return viz::mojom::ResourceSource::kUnknown;
    case viz::TransferableResource::ResourceSource::kAR:
      return viz::mojom::ResourceSource::kAR;
    case viz::TransferableResource::ResourceSource::kCanvas:
      return viz::mojom::ResourceSource::kCanvas;
    case viz::TransferableResource::ResourceSource::kDrawingBuffer:
      return viz::mojom::ResourceSource::kDrawingBuffer;
    case viz::TransferableResource::ResourceSource::kExoBuffer:
      return viz::mojom::ResourceSource::kExoBuffer;
    case viz::TransferableResource::ResourceSource::kHeadsUpDisplay:
      return viz::mojom::ResourceSource::kHeadsUpDisplay;
    case viz::TransferableResource::ResourceSource::kImageLayerBridge:
      return viz::mojom::ResourceSource::kImageLayerBridge;
    case viz::TransferableResource::ResourceSource::kPPBGraphics3D:
      return viz::mojom::ResourceSource::kPPBGraphics3D;
    case viz::TransferableResource::ResourceSource::kPepperGraphics2D:
      return viz::mojom::ResourceSource::kPepperGraphics2D;
    case viz::TransferableResource::ResourceSource::kViewTransition:
      return viz::mojom::ResourceSource::kViewTransition;
    case viz::TransferableResource::ResourceSource::kStaleContent:
      return viz::mojom::ResourceSource::kStaleContent;
    case viz::TransferableResource::ResourceSource::kTest:
      return viz::mojom::ResourceSource::kTest;
    case viz::TransferableResource::ResourceSource::kTileRasterTask:
      return viz::mojom::ResourceSource::kTileRasterTask;
    case viz::TransferableResource::ResourceSource::kUI:
      return viz::mojom::ResourceSource::kUI;
    case viz::TransferableResource::ResourceSource::kVideo:
      return viz::mojom::ResourceSource::kVideo;
    case viz::TransferableResource::ResourceSource::kWebGPUSwapBuffer:
      return viz::mojom::ResourceSource::kWebGPUSwapBuffer;
  }
  NOTREACHED();
}

// static
viz::TransferableResource::ResourceSource
EnumTraits<viz::mojom::ResourceSource,
           viz::TransferableResource::ResourceSource>::
    FromMojom(viz::mojom::ResourceSource input) {
  switch (input) {
    case viz::mojom::ResourceSource::kUnknown:
      return viz::TransferableResource::ResourceSource::kUnknown;
    case viz::mojom::ResourceSource::kAR:
      return viz::TransferableResource::ResourceSource::kAR;
    case viz::mojom::ResourceSource::kCanvas:
      return viz::TransferableResource::ResourceSource::kCanvas;
    case viz::mojom::ResourceSource::kDrawingBuffer:
      return viz::TransferableResource::ResourceSource::kDrawingBuffer;
    case viz::mojom::ResourceSource::kExoBuffer:
      return viz::TransferableResource::ResourceSource::kExoBuffer;
    case viz::mojom::ResourceSource::kHeadsUpDisplay:
      return viz::TransferableResource::ResourceSource::kHeadsUpDisplay;
    case viz::mojom::ResourceSource::kImageLayerBridge:
      return viz::TransferableResource::ResourceSource::kImageLayerBridge;
    case viz::mojom::ResourceSource::kPPBGraphics3D:
      return viz::TransferableResource::ResourceSource::kPPBGraphics3D;
    case viz::mojom::ResourceSource::kPepperGraphics2D:
      return viz::TransferableResource::ResourceSource::kPepperGraphics2D;
    case viz::mojom::ResourceSource::kViewTransition:
      return viz::TransferableResource::ResourceSource::kViewTransition;
    case viz::mojom::ResourceSource::kStaleContent:
      return viz::TransferableResource::ResourceSource::kStaleContent;
    case viz::mojom::ResourceSource::kTest:
      return viz::TransferableResource::ResourceSource::kTest;
    case viz::mojom::ResourceSource::kTileRasterTask:
      return viz::TransferableResource::ResourceSource::kTileRasterTask;
    case viz::mojom::ResourceSource::kUI:
      return viz::TransferableResource::ResourceSource::kUI;
    case viz::mojom::ResourceSource::kVideo:
      return viz::TransferableResource::ResourceSource::kVideo;
    case viz::mojom::ResourceSource::kWebGPUSwapBuffer:
      return viz::TransferableResource::ResourceSource::kWebGPUSwapBuffer;
  }
  NOTREACHED();
}

// static
bool StructTraits<viz::mojom::TransferableResourceDataView,
                  viz::TransferableResource>::
    Read(viz::mojom::TransferableResourceDataView data,
         viz::TransferableResource* out) {
  viz::ResourceId id;

  gpu::SyncToken sync_token;
  gpu::ExportedSharedImage exported_shared_image;
  viz::TransferableResource::MetadataOverride metadata_override;

  if (!data.ReadSharedImage(&exported_shared_image) ||
      !data.ReadSyncToken(&sync_token) ||
      !data.ReadMetadataOverride(&metadata_override) ||
      !data.ReadHdrMetadata(&out->hdr_metadata) || !data.ReadId(&id) ||
      !data.ReadSynchronizationType(&out->synchronization_type) ||
      !data.ReadResourceSource(&out->resource_source)) {
    return false;
  }
#if BUILDFLAG(IS_ANDROID)
  if (!data.ReadYcbcrInfo(&out->ycbcr_info)) {
    return false;
  }
#endif

  out->id = id;
  out->set_shared_image(
      gpu::ClientSharedImage::ImportUnowned(std::move(exported_shared_image)));
  out->set_sync_token(sync_token);
  out->set_metadata_override(metadata_override);
  out->is_low_latency_rendering = data.is_low_latency_rendering();
  out->needs_detiling = data.needs_detiling();

#if BUILDFLAG(IS_ANDROID)
  out->is_backed_by_surface_view = data.is_backed_by_surface_view();
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  out->wants_promotion_hint = data.wants_promotion_hint();
#endif

  return true;
}

}  // namespace mojo
