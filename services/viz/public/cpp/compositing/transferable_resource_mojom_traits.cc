// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

// static
viz::mojom::ResourceFormat
EnumTraits<viz::mojom::ResourceFormat, viz::ResourceFormat>::ToMojom(
    viz::ResourceFormat format) {
  switch (format) {
    case viz::ResourceFormat::RGBA_8888:
      return viz::mojom::ResourceFormat::RGBA_8888;
    case viz::ResourceFormat::RGBA_4444:
      return viz::mojom::ResourceFormat::RGBA_4444;
    case viz::ResourceFormat::BGRA_8888:
      return viz::mojom::ResourceFormat::BGRA_8888;
    case viz::ResourceFormat::ALPHA_8:
      return viz::mojom::ResourceFormat::ALPHA_8;
    case viz::ResourceFormat::LUMINANCE_8:
      return viz::mojom::ResourceFormat::LUMINANCE_8;
    case viz::ResourceFormat::RGB_565:
      return viz::mojom::ResourceFormat::RGB_565;
    case viz::ResourceFormat::BGR_565:
      return viz::mojom::ResourceFormat::BGR_565;
    case viz::ResourceFormat::ETC1:
      return viz::mojom::ResourceFormat::ETC1;
    case viz::ResourceFormat::RED_8:
      return viz::mojom::ResourceFormat::RED_8;
    case viz::ResourceFormat::RG_88:
      return viz::mojom::ResourceFormat::RG_88;
    case viz::ResourceFormat::LUMINANCE_F16:
      return viz::mojom::ResourceFormat::LUMINANCE_F16;
    case viz::ResourceFormat::RGBA_F16:
      return viz::mojom::ResourceFormat::RGBA_F16;
    case viz::ResourceFormat::R16_EXT:
      return viz::mojom::ResourceFormat::R16_EXT;
    case viz::ResourceFormat::RG16_EXT:
      return viz::mojom::ResourceFormat::RG16_EXT;
    case viz::ResourceFormat::RGBX_8888:
      return viz::mojom::ResourceFormat::RGBX_8888;
    case viz::ResourceFormat::BGRX_8888:
      return viz::mojom::ResourceFormat::BGRX_8888;
    case viz::ResourceFormat::RGBA_1010102:
      return viz::mojom::ResourceFormat::RGBX_1010102;
    case viz::ResourceFormat::BGRA_1010102:
      return viz::mojom::ResourceFormat::BGRX_1010102;
    case viz::ResourceFormat::YVU_420:
      return viz::mojom::ResourceFormat::YVU_420;
    case viz::ResourceFormat::YUV_420_BIPLANAR:
      return viz::mojom::ResourceFormat::YUV_420_BIPLANAR;
    case viz::ResourceFormat::P010:
      return viz::mojom::ResourceFormat::P010;
  }
  NOTREACHED();
  return viz::mojom::ResourceFormat::RGBA_8888;
}

// static
bool EnumTraits<viz::mojom::ResourceFormat, viz::ResourceFormat>::FromMojom(
    viz::mojom::ResourceFormat format,
    viz::ResourceFormat* out) {
  switch (format) {
    case viz::mojom::ResourceFormat::RGBA_8888:
      *out = viz::ResourceFormat::RGBA_8888;
      return true;
    case viz::mojom::ResourceFormat::RGBA_4444:
      *out = viz::ResourceFormat::RGBA_4444;
      return true;
    case viz::mojom::ResourceFormat::BGRA_8888:
      *out = viz::ResourceFormat::BGRA_8888;
      return true;
    case viz::mojom::ResourceFormat::ALPHA_8:
      *out = viz::ResourceFormat::ALPHA_8;
      return true;
    case viz::mojom::ResourceFormat::LUMINANCE_8:
      *out = viz::ResourceFormat::LUMINANCE_8;
      return true;
    case viz::mojom::ResourceFormat::RGB_565:
      *out = viz::ResourceFormat::RGB_565;
      return true;
    case viz::mojom::ResourceFormat::BGR_565:
      *out = viz::ResourceFormat::BGR_565;
      return true;
    case viz::mojom::ResourceFormat::ETC1:
      *out = viz::ResourceFormat::ETC1;
      return true;
    case viz::mojom::ResourceFormat::RED_8:
      *out = viz::ResourceFormat::RED_8;
      return true;
    case viz::mojom::ResourceFormat::RG_88:
      *out = viz::ResourceFormat::RG_88;
      return true;
    case viz::mojom::ResourceFormat::LUMINANCE_F16:
      *out = viz::ResourceFormat::LUMINANCE_F16;
      return true;
    case viz::mojom::ResourceFormat::RGBA_F16:
      *out = viz::ResourceFormat::RGBA_F16;
      return true;
    case viz::mojom::ResourceFormat::R16_EXT:
      *out = viz::ResourceFormat::R16_EXT;
      return true;
    case viz::mojom::ResourceFormat::RG16_EXT:
      *out = viz::ResourceFormat::RG16_EXT;
      return true;
    case viz::mojom::ResourceFormat::RGBX_8888:
      *out = viz::ResourceFormat::RGBX_8888;
      return true;
    case viz::mojom::ResourceFormat::BGRX_8888:
      *out = viz::ResourceFormat::BGRX_8888;
      return true;
    case viz::mojom::ResourceFormat::RGBX_1010102:
      *out = viz::ResourceFormat::RGBA_1010102;
      return true;
    case viz::mojom::ResourceFormat::BGRX_1010102:
      *out = viz::ResourceFormat::BGRA_1010102;
      return true;
    case viz::mojom::ResourceFormat::YVU_420:
      *out = viz::ResourceFormat::YVU_420;
      return true;
    case viz::mojom::ResourceFormat::YUV_420_BIPLANAR:
      *out = viz::ResourceFormat::YUV_420_BIPLANAR;
      return true;
    case viz::mojom::ResourceFormat::P010:
      *out = viz::ResourceFormat::P010;
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
  if (!data.ReadSize(&out->size) || !data.ReadFormat(&out->format) ||
      !data.ReadMailboxHolder(&out->mailbox_holder) ||
      !data.ReadColorSpace(&out->color_space) ||
      !data.ReadYcbcrInfo(&out->ycbcr_info) || !data.ReadId(&id)) {
    return false;
  }
  out->id = id;
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
