// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "services/viz/public/cpp/compositing/quads_mojom_traits.h"

#include <algorithm>
#include <optional>

#include "base/notreached.h"
#include "cc/mojom/paint_flags_mojom_traits.h"
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
  NOTREACHED();
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
    viz::SetDeserializationCrashKeyString(
        "Failed read RoundedDisplayMasksInfo::radii");
    return false;
  }

  info->is_horizontally_positioned = data.is_horizontally_positioned();
  return true;
}

// static
bool StructTraits<viz::mojom::DebugBorderQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::DebugBorderQuadStateDataView data, viz::DrawQuad* out) {
  viz::DebugBorderDrawQuad* quad = static_cast<viz::DebugBorderDrawQuad*>(out);
  if (!data.ReadColor(&quad->color)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read DebugBorderDrawQuad::color");
    return false;
  }
  quad->width = data.width();
  return true;
}

// static
bool StructTraits<
    viz::mojom::CompositorRenderPassQuadStateDataView,
    viz::DrawQuad>::Read(viz::mojom::CompositorRenderPassQuadStateDataView data,
                         viz::DrawQuad* out) {
  auto* quad = static_cast<viz::CompositorRenderPassDrawQuad*>(out);
  if (!data.ReadMaskUvRect(&quad->mask_uv_rect)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::mask_uv_rect");
    return false;
  }
  if (!data.ReadMaskTextureSize(&quad->mask_texture_size)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::mask_texture_size");
    return false;
  }
  if (!data.ReadFiltersScale(&quad->filters_scale)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::filters_scale");
    return false;
  }
  if (!data.ReadFiltersOrigin(&quad->filters_origin)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::filters_origin");
    return false;
  }
  if (!data.ReadRenderPassId(&quad->render_pass_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::render_pass_id");
    return false;
  }
  if (!data.ReadMaskResourceId(&quad->resource_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorRenderPassDrawQuad::resource_id");
    return false;
  }

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
  if (!data.ReadColor(&quad->color)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SolidColorDrawQuad::color");
    return false;
  }
  // Clamp the alpha component of the color to the range of [0, 1].
  quad->color.fA = std::clamp(quad->color.fA, 0.0f, 1.0f);
  return true;
}

// static
bool StructTraits<viz::mojom::SurfaceQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SurfaceQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::SurfaceDrawQuad* quad = static_cast<viz::SurfaceDrawQuad*>(out);
  if (!data.ReadDefaultBackgroundColor(&quad->default_background_color)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SurfaceDrawQuad::default_background_color");
    return false;
  }
  if (!data.ReadOverrideChildFilterQuality(
          &quad->override_child_filter_quality)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SurfaceDrawQuad::override_child_filter_quality");
    return false;
  }
  if (!data.ReadOverrideChildDynamicRangeLimit(
          &quad->override_child_dynamic_range_limit)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SurfaceDrawQuad::override_child_dynamic_range_limit");
    return false;
  }
  if (!data.ReadSurfaceRange(&quad->surface_range)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SurfaceDrawQuad::surface_range");
    return false;
  }
  quad->stretch_content_to_fill_bounds = data.stretch_content_to_fill_bounds();
  quad->is_reflection = data.is_reflection();
  quad->allow_merge = data.allow_merge();
  return true;
}

// static
bool StructTraits<viz::mojom::TextureQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TextureQuadStateDataView data,
    viz::DrawQuad* out) {
  auto* quad = static_cast<viz::TextureDrawQuad*>(out);

  if (!data.ReadResourceId(&quad->resource_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::resource_id");
    return false;
  }

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  viz::OverlayPriority overlay_priority_hint = viz::OverlayPriority::kLow;
  if (!data.ReadTexCoordRect(&quad->tex_coord_rect_)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::tex_coord_rect");
    return false;
  }
  if (!data.ReadProtectedVideoType(&protected_video_type)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::protected_video_type");
    return false;
  }
  if (!data.ReadOverlayPriorityHint(&overlay_priority_hint)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::overlay_priority_hint");
    return false;
  }
  if (!data.ReadRoundedDisplayMasksInfo(&quad->rounded_display_masks_info)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::rounded_display_masks_info");
    return false;
  }
  if (!data.ReadDynamicRangeLimit(&quad->dynamic_range_limit)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::dynamic_range_limit");
    return false;
  }

  // The texture coordinate rect must have non-negative width and height.
  if (quad->tex_coord_rect_.width() < 0 || quad->tex_coord_rect_.height() < 0) {
    viz::SetDeserializationCrashKeyString("Draw quad invalid tex coord rect");
    return false;
  }

  quad->protected_video_type = protected_video_type;
  quad->overlay_priority_hint = overlay_priority_hint;
  if (!data.ReadBackgroundColor(&quad->background_color)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::background_color");
    return false;
  }

  quad->nearest_neighbor = data.nearest_neighbor();
  quad->secure_output_only = data.secure_output_only();
  quad->is_video_frame = data.is_video_frame();
  quad->force_rgbx = data.force_rgbx();
  quad->is_normalized_coords = data.is_normalized_coords();
  if (quad->is_normalized_coords) {
    // If the texture coordinates are normalized, they must be in the range
    // [0, 1], we've already checked above zero above.
    const bool is_tex_coord_rect_in_range =
        quad->tex_coord_rect_.width() <= 1.0f &&
        quad->tex_coord_rect_.height() <= 1.0f;
    if (!is_tex_coord_rect_in_range) {
      viz::SetDeserializationCrashKeyString(
          "Draw quad invalid normalized tex coord rect");
      return false;
    }
  }

  if (!data.ReadDamageRect(&quad->damage_rect)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TextureDrawQuad::damage_rect");
    return false;
  }

  return true;
}

// static
bool StructTraits<viz::mojom::TileQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TileQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::TileDrawQuad* quad = static_cast<viz::TileDrawQuad*>(out);
  if (!data.ReadTexCoordRect(&quad->tex_coord_rect)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TileDrawQuad::tex_coord_rect");
    return false;
  }
  if (!data.ReadResourceId(&quad->resource_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read TileDrawQuad::resource_id");
    return false;
  }

  quad->nearest_neighbor = data.nearest_neighbor();
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  return true;
}

// static
bool StructTraits<viz::mojom::SharedElementQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::SharedElementQuadStateDataView data, viz::DrawQuad* out) {
  viz::SharedElementDrawQuad* shared_element_quad =
      static_cast<viz::SharedElementDrawQuad*>(out);
  if (!data.ReadElementResourceId(&shared_element_quad->element_resource_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read SharedElementDrawQuad::element_resource_id");
    return false;
  }
  if (!shared_element_quad->element_resource_id.IsValid()) {
    viz::SetDeserializationCrashKeyString(
        "Invalid ViewTransitionElementResourceId in SharedElementDrawQuad");
    return false;
  }
  return true;
}

// static
bool StructTraits<viz::mojom::VideoHoleQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::VideoHoleQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::VideoHoleDrawQuad* video_hole_quad =
      static_cast<viz::VideoHoleDrawQuad*>(out);
  if (!data.ReadOverlayPlaneId(&video_hole_quad->overlay_plane_id)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read VideoHoleDrawQuad::overlay_plane_id");
    return false;
  }
  return true;
}

// static
bool StructTraits<viz::mojom::DrawQuadDataView, viz::DrawQuad>::Read(
    viz::mojom::DrawQuadDataView data,
    viz::DrawQuad* out) {
  if (!data.ReadRect(&out->rect)) {
    viz::SetDeserializationCrashKeyString("Failed read DrawQuad::rect");
    return false;
  }
  if (!data.ReadVisibleRect(&out->visible_rect)) {
    viz::SetDeserializationCrashKeyString("Failed read DrawQuad::visible_rect");
    return false;
  }
  if (!out->rect.Contains(out->visible_rect)) {
    viz::SetDeserializationCrashKeyString("Rect does not contain visible rect");
    return false;
  }

  out->needs_blending = data.needs_blending();
  if (!data.ReadDrawQuadState(out)) {
    viz::SetDeserializationCrashKeyString("Failed read DrawQuad state");
    return false;
  }
  return true;
}

}  // namespace mojo
