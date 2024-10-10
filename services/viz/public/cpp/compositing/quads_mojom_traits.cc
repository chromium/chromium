// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/viz/public/cpp/compositing/quads_mojom_traits.h"

#include <algorithm>
#include <optional>

#include "base/notreached.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

viz::DrawQuad* AllocateAndConstruct(
    viz::mojom::DrawQuadStateDataView::Tag material,
    viz::QuadList* list) {
  viz::DrawQuad* quad = nullptr;
  switch (material) {
    case viz::mojom::DrawQuadStateDataView::Tag::kDebugBorderQuadState:
      quad = list->AllocateAndConstruct<viz::DebugBorderDrawQuad>();
      quad->material = viz::DrawQuad::Material::kDebugBorder;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kRenderPassQuadState:
      quad = list->AllocateAndConstruct<viz::CompositorRenderPassDrawQuad>();
      quad->material = viz::DrawQuad::Material::kCompositorRenderPass;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kSolidColorQuadState:
      quad = list->AllocateAndConstruct<viz::SolidColorDrawQuad>();
      quad->material = viz::DrawQuad::Material::kSolidColor;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kSurfaceQuadState:
      quad = list->AllocateAndConstruct<viz::SurfaceDrawQuad>();
      quad->material = viz::DrawQuad::Material::kSurfaceContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kTextureQuadState:
      quad = list->AllocateAndConstruct<viz::TextureDrawQuad>();
      quad->material = viz::DrawQuad::Material::kTextureContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kTileQuadState:
      quad = list->AllocateAndConstruct<viz::TileDrawQuad>();
      quad->material = viz::DrawQuad::Material::kTiledContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kVideoHoleQuadState:
      quad = list->AllocateAndConstruct<viz::VideoHoleDrawQuad>();
      quad->material = viz::DrawQuad::Material::kVideoHole;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::kSharedElementQuadState:
      quad = list->AllocateAndConstruct<viz::SharedElementDrawQuad>();
      quad->material = viz::DrawQuad::Material::kSharedElement;
      return quad;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
bool StructTraits<viz::mojom::RoundedDisplayMasksInfoDataView,
                  viz::TextureDrawQuad::RoundedDisplayMasksInfo>::
    Read(viz::mojom::RoundedDisplayMasksInfoDataView data,
         viz::TextureDrawQuad::RoundedDisplayMasksInfo* out) {
  viz::TextureDrawQuad::RoundedDisplayMasksInfo* info =
      static_cast<viz::TextureDrawQuad::RoundedDisplayMasksInfo*>(out);
  base::span<uint8_t> radii_array(info->radii);
  if (!data.ReadRadii(&radii_array)) {
    return false;
  }

  info->is_horizontally_positioned = data.is_horizontally_positioned();
  return true;
}

// static
bool StructTraits<viz::mojom::DebugBorderQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::DebugBorderQuadStateDataView data, viz::DrawQuad* out) {
  viz::DebugBorderDrawQuad* quad = static_cast<viz::DebugBorderDrawQuad*>(out);
  if (!data.ReadColor(&quad->color))
    return false;
  quad->width = data.width();
  return true;
}

// static
bool StructTraits<
    viz::mojom::CompositorRenderPassQuadStateDataView,
    viz::DrawQuad>::Read(viz::mojom::CompositorRenderPassQuadStateDataView data,
                         viz::DrawQuad* out) {
  auto* quad = static_cast<viz::CompositorRenderPassDrawQuad*>(out);
  viz::ResourceId& mask_resource_id =
      quad->resources
          .ids[viz::CompositorRenderPassDrawQuad::kMaskResourceIdIndex];
  if (!data.ReadMaskUvRect(&quad->mask_uv_rect) ||
      !data.ReadMaskTextureSize(&quad->mask_texture_size) ||
      !data.ReadFiltersScale(&quad->filters_scale) ||
      !data.ReadFiltersOrigin(&quad->filters_origin) ||
      !data.ReadTexCoordRect(&quad->tex_coord_rect) ||
      !data.ReadRenderPassId(&quad->render_pass_id) ||
      !data.ReadMaskResourceId(&mask_resource_id)) {
    return false;
  }
  quad->resources.count = mask_resource_id ? 1 : 0;

  // CompositorRenderPass ids are never zero.
  if (!quad->render_pass_id) {
    viz::SetDeserializationCrashKeyString("Draw quad invalid render pass ID");
    return false;
  }
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  quad->backdrop_filter_quality = data.backdrop_filter_quality();
  quad->intersects_damage_under = data.intersects_damage_under();
  return true;
}

// static
bool StructTraits<viz::mojom::SolidColorQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SolidColorQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::SolidColorDrawQuad* quad = static_cast<viz::SolidColorDrawQuad*>(out);
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  if (!data.ReadColor(&quad->color))
    return false;
  // Clamp the alpha component of the color to the range of [0, 1].
  quad->color.fA = std::clamp(quad->color.fA, 0.0f, 1.0f);
  return true;
}

// static
bool StructTraits<viz::mojom::SurfaceQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SurfaceQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::SurfaceDrawQuad* quad = static_cast<viz::SurfaceDrawQuad*>(out);
  if (!data.ReadDefaultBackgroundColor(&quad->default_background_color))
    return false;
  quad->stretch_content_to_fill_bounds = data.stretch_content_to_fill_bounds();
  quad->is_reflection = data.is_reflection();
  quad->allow_merge = data.allow_merge();
  return data.ReadSurfaceRange(&quad->surface_range);
}

// static
bool StructTraits<viz::mojom::TextureQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TextureQuadStateDataView data,
    viz::DrawQuad* out) {
  auto* quad = static_cast<viz::TextureDrawQuad*>(out);

  if (!data.ReadResourceId(
          &quad->resources.ids[viz::TextureDrawQuad::kResourceIdIndex]) ||
      !data.ReadResourceSizeInPixels(&quad->overlay_resources.size_in_pixels)) {
    return false;
  }

  quad->resources.count = 1;
  quad->premultiplied_alpha = data.premultiplied_alpha();
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  viz::OverlayPriority overlay_priority_hint = viz::OverlayPriority::kLow;
  if (!data.ReadUvTopLeft(&quad->uv_top_left) ||
      !data.ReadUvBottomRight(&quad->uv_bottom_right) ||
      !data.ReadProtectedVideoType(&protected_video_type) ||
      !data.ReadOverlayPriorityHint(&overlay_priority_hint) ||
      !data.ReadRoundedDisplayMasksInfo(&quad->rounded_display_masks_info)) {
    return false;
  }
  quad->protected_video_type = protected_video_type;
  quad->overlay_priority_hint = overlay_priority_hint;
  if (!data.ReadBackgroundColor(&quad->background_color))
    return false;

  quad->y_flipped = data.y_flipped();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->secure_output_only = data.secure_output_only();
  quad->is_stream_video = data.is_stream_video();
  quad->is_video_frame = data.is_video_frame();
  quad->force_rgbx = data.force_rgbx();

  if (!data.ReadDamageRect(&quad->damage_rect))
    return false;

  return true;
}

// static
bool StructTraits<viz::mojom::TileQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TileQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::TileDrawQuad* quad = static_cast<viz::TileDrawQuad*>(out);
  if (!data.ReadTexCoordRect(&quad->tex_coord_rect) ||
      !data.ReadTextureSize(&quad->texture_size) ||
      !data.ReadResourceId(
          &quad->resources.ids[viz::TileDrawQuad::kResourceIdIndex])) {
    return false;
  }

  quad->is_premultiplied = data.is_premultiplied();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  quad->resources.count = 1;
  return true;
}

// static
bool StructTraits<viz::mojom::SharedElementQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::SharedElementQuadStateDataView data, viz::DrawQuad* out) {
  viz::SharedElementDrawQuad* shared_element_quad =
      static_cast<viz::SharedElementDrawQuad*>(out);
  return data.ReadResourceId(&shared_element_quad->resource_id);
}

// static
bool StructTraits<viz::mojom::VideoHoleQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::VideoHoleQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::VideoHoleDrawQuad* video_hole_quad =
      static_cast<viz::VideoHoleDrawQuad*>(out);
  return data.ReadOverlayPlaneId(&video_hole_quad->overlay_plane_id);
}

// static
bool StructTraits<viz::mojom::DrawQuadDataView, viz::DrawQuad>::Read(
    viz::mojom::DrawQuadDataView data,
    viz::DrawQuad* out) {
  if (!data.ReadRect(&out->rect) || !data.ReadVisibleRect(&out->visible_rect)) {
    return false;
  }
  if (!out->rect.Contains(out->visible_rect)) {
    viz::SetDeserializationCrashKeyString("Rect does not contain visible rect");
    return false;
  }

  out->needs_blending = data.needs_blending();
  return data.ReadDrawQuadState(out);
}

}  // namespace mojo
