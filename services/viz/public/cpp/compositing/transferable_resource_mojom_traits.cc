// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include "build/build_config.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

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
bool EnumTraits<viz::mojom::SynchronizationType,
                viz::TransferableResource::SynchronizationType>::
    FromMojom(viz::mojom::SynchronizationType input,
              viz::TransferableResource::SynchronizationType* out) {
  switch (input) {
    case viz::mojom::SynchronizationType::kSyncToken:
      *out = viz::TransferableResource::SynchronizationType::kSyncToken;
      return true;
    case viz::mojom::SynchronizationType::kGpuCommandsCompleted:
      *out =
          viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
      return true;
    case viz::mojom::SynchronizationType::kReleaseFence:
      *out = viz::TransferableResource::SynchronizationType::kReleaseFence;
      return true;
  }
  return false;
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
bool EnumTraits<viz::mojom::ResourceSource,
                viz::TransferableResource::ResourceSource>::
    FromMojom(viz::mojom::ResourceSource input,
              viz::TransferableResource::ResourceSource* out) {
  switch (input) {
    case viz::mojom::ResourceSource::kUnknown:
      *out = viz::TransferableResource::ResourceSource::kUnknown;
      return true;
    case viz::mojom::ResourceSource::kAR:
      *out = viz::TransferableResource::ResourceSource::kAR;
      return true;
    case viz::mojom::ResourceSource::kCanvas:
      *out = viz::TransferableResource::ResourceSource::kCanvas;
      return true;
    case viz::mojom::ResourceSource::kDrawingBuffer:
      *out = viz::TransferableResource::ResourceSource::kDrawingBuffer;
      return true;
    case viz::mojom::ResourceSource::kExoBuffer:
      *out = viz::TransferableResource::ResourceSource::kExoBuffer;
      return true;
    case viz::mojom::ResourceSource::kHeadsUpDisplay:
      *out = viz::TransferableResource::ResourceSource::kHeadsUpDisplay;
      return true;
    case viz::mojom::ResourceSource::kImageLayerBridge:
      *out = viz::TransferableResource::ResourceSource::kImageLayerBridge;
      return true;
    case viz::mojom::ResourceSource::kPPBGraphics3D:
      *out = viz::TransferableResource::ResourceSource::kPPBGraphics3D;
      return true;
    case viz::mojom::ResourceSource::kPepperGraphics2D:
      *out = viz::TransferableResource::ResourceSource::kPepperGraphics2D;
      return true;
    case viz::mojom::ResourceSource::kViewTransition:
      *out = viz::TransferableResource::ResourceSource::kViewTransition;
      return true;
    case viz::mojom::ResourceSource::kStaleContent:
      *out = viz::TransferableResource::ResourceSource::kStaleContent;
      return true;
    case viz::mojom::ResourceSource::kTest:
      *out = viz::TransferableResource::ResourceSource::kTest;
      return true;
    case viz::mojom::ResourceSource::kTileRasterTask:
      *out = viz::TransferableResource::ResourceSource::kTileRasterTask;
      return true;
    case viz::mojom::ResourceSource::kUI:
      *out = viz::TransferableResource::ResourceSource::kUI;
      return true;
    case viz::mojom::ResourceSource::kVideo:
      *out = viz::TransferableResource::ResourceSource::kVideo;
      return true;
    case viz::mojom::ResourceSource::kWebGPUSwapBuffer:
      *out = viz::TransferableResource::ResourceSource::kWebGPUSwapBuffer;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::TransferableResourceDataView,
                  viz::TransferableResource>::
    Read(viz::mojom::TransferableResourceDataView data,
         viz::TransferableResource* out) {
  viz::ResourceId id;

  gpu::SyncToken sync_token;
  gpu::Mailbox memory_buffer_id;
  if (!data.ReadSize(&out->size) || !data.ReadFormat(&out->format) ||
      !data.ReadMemoryBufferId(&memory_buffer_id) ||
      !data.ReadSyncToken(&sync_token) ||
      !data.ReadColorSpace(&out->color_space) ||
      !data.ReadHdrMetadata(&out->hdr_metadata) || !data.ReadId(&id) ||
      !data.ReadSynchronizationType(&out->synchronization_type) ||
      !data.ReadOrigin(&out->origin) || !data.ReadAlphaType(&out->alpha_type) ||
      !data.ReadResourceSource(&out->resource_source)) {
    return false;
  }
#if BUILDFLAG(IS_ANDROID)
  if (!data.ReadYcbcrInfo(&out->ycbcr_info)) {
    return false;
  }
#endif

  out->id = id;
  out->is_software = data.is_software();
  out->set_memory_buffer_id(memory_buffer_id);
  out->set_sync_token(sync_token);
  out->set_texture_target(data.texture_target());
  out->is_overlay_candidate = data.is_overlay_candidate();
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
