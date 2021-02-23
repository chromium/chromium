// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/quads_mojom_traits.h"

#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

viz::DrawQuad* AllocateAndConstruct(
    viz::mojom::DrawQuadStateDataView::Tag material,
    viz::QuadList* list) {
  viz::DrawQuad* quad = nullptr;
  switch (material) {
    case viz::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::DebugBorderDrawQuad>();
      quad->material = viz::DrawQuad::Material::kDebugBorder;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::RENDER_PASS_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::CompositorRenderPassDrawQuad>();
      quad->material = viz::DrawQuad::Material::kCompositorRenderPass;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::SOLID_COLOR_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::SolidColorDrawQuad>();
      quad->material = viz::DrawQuad::Material::kSolidColor;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::STREAM_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::StreamVideoDrawQuad>();
      quad->material = viz::DrawQuad::Material::kStreamVideoContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::SURFACE_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::SurfaceDrawQuad>();
      quad->material = viz::DrawQuad::Material::kSurfaceContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::TEXTURE_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::TextureDrawQuad>();
      quad->material = viz::DrawQuad::Material::kTextureContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::TILE_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::TileDrawQuad>();
      quad->material = viz::DrawQuad::Material::kTiledContent;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::VIDEO_HOLE_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::VideoHoleDrawQuad>();
      quad->material = viz::DrawQuad::Material::kVideoHole;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::YUV_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<viz::YUVVideoDrawQuad>();
      quad->material = viz::DrawQuad::Material::kYuvVideoContent;
      return quad;
  }
  NOTREACHED();
  return nullptr;
}

// static
bool StructTraits<viz::mojom::DebugBorderQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::DebugBorderQuadStateDataView data, viz::DrawQuad* out) {
  viz::DebugBorderDrawQuad* quad = static_cast<viz::DebugBorderDrawQuad*>(out);
  quad->color = data.color();
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
  quad->color = data.color();
  return true;
}

// static
bool StructTraits<viz::mojom::StreamVideoQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::StreamVideoQuadStateDataView data, viz::DrawQuad* out) {
  auto* quad = static_cast<viz::StreamVideoDrawQuad*>(out);
  quad->resources.count = 1;
  return data.ReadResourceSizeInPixels(
             &quad->overlay_resources.size_in_pixels) &&
         data.ReadUvTopLeft(&quad->uv_top_left) &&
         data.ReadUvBottomRight(&quad->uv_bottom_right) &&
         data.ReadResourceId(
             &quad->resources.ids[viz::StreamVideoDrawQuad::kResourceIdIndex]);
}

// static
bool StructTraits<viz::mojom::SurfaceQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SurfaceQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::SurfaceDrawQuad* quad = static_cast<viz::SurfaceDrawQuad*>(out);
  quad->default_background_color = data.default_background_color();
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
  if (!data.ReadUvTopLeft(&quad->uv_top_left) ||
      !data.ReadUvBottomRight(&quad->uv_bottom_right) ||
      !data.ReadProtectedVideoType(&protected_video_type)) {
    return false;
  }
  quad->protected_video_type = protected_video_type;
  quad->background_color = data.background_color();
  base::span<float> vertex_opacity_array(quad->vertex_opacity);
  if (!data.ReadVertexOpacity(&vertex_opacity_array))
    return false;

  quad->y_flipped = data.y_flipped();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->secure_output_only = data.secure_output_only();
  quad->is_video_frame = data.is_video_frame();
  quad->hw_protected_validation_id = data.hw_protected_validation_id();
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
bool StructTraits<viz::mojom::VideoHoleQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::VideoHoleQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::VideoHoleDrawQuad* video_hole_quad =
      static_cast<viz::VideoHoleDrawQuad*>(out);
  return data.ReadOverlayPlaneId(&video_hole_quad->overlay_plane_id);
}

// static
bool StructTraits<viz::mojom::YUVVideoQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::YUVVideoQuadStateDataView data,
    viz::DrawQuad* out) {
  viz::YUVVideoDrawQuad* quad = static_cast<viz::YUVVideoDrawQuad*>(out);
  if (!data.ReadYaTexCoordRect(&quad->ya_tex_coord_rect) ||
      !data.ReadUvTexCoordRect(&quad->uv_tex_coord_rect) ||
      !data.ReadYaTexSize(&quad->ya_tex_size) ||
      !data.ReadUvTexSize(&quad->uv_tex_size) ||
      !data.ReadVideoColorSpace(&quad->video_color_space) ||
      !data.ReadProtectedVideoType(&quad->protected_video_type) ||
      !data.ReadHdrMetadata(&quad->hdr_metadata) ||
      !data.ReadYPlaneResourceId(
          &quad->resources
               .ids[viz::YUVVideoDrawQuad::kYPlaneResourceIdIndex]) ||
      !data.ReadUPlaneResourceId(
          &quad->resources
               .ids[viz::YUVVideoDrawQuad::kUPlaneResourceIdIndex]) ||
      !data.ReadVPlaneResourceId(
          &quad->resources
               .ids[viz::YUVVideoDrawQuad::kVPlaneResourceIdIndex]) ||
      !data.ReadAPlaneResourceId(
          &quad->resources
               .ids[viz::YUVVideoDrawQuad::kAPlaneResourceIdIndex])) {
    return false;
  }
  static_assert(viz::YUVVideoDrawQuad::kAPlaneResourceIdIndex ==
                    viz::DrawQuad::Resources::kMaxResourceIdCount - 1,
                "The A plane resource should be the last resource ID.");
  quad->resources.count =
      quad->resources.ids[viz::YUVVideoDrawQuad::kAPlaneResourceIdIndex] ? 4
                                                                         : 3;

  quad->resource_offset = data.resource_offset();
  quad->resource_multiplier = data.resource_multiplier();
  quad->bits_per_channel = data.bits_per_channel();
  if (quad->bits_per_channel < viz::YUVVideoDrawQuad::kMinBitsPerChannel) {
    viz::SetDeserializationCrashKeyString("Bits per channel too small");
    return false;
  }
  if (quad->bits_per_channel > viz::YUVVideoDrawQuad::kMaxBitsPerChannel) {
    viz::SetDeserializationCrashKeyString("Bits per channel too big");
    return false;
  }
  return true;
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
