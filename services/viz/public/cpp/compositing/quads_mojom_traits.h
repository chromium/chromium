// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_QUADS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_QUADS_MOJOM_TRAITS_H_

#include <optional>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "services/viz/public/cpp/compositing/filter_operation_mojom_traits.h"
#include "services/viz/public/cpp/compositing/filter_operations_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_quad_state_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_range_mojom_traits.h"
#include "services/viz/public/cpp/compositing/view_transition_element_resource_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/quads.mojom-shared.h"
#include "skia/public/mojom/skcolor4f_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

viz::DrawQuad* AllocateAndConstruct(
    viz::mojom::DrawQuadStateDataView::Tag material,
    viz::QuadList* list);

template <>
struct EnumTraits<viz::mojom::ProtectedVideoState, gfx::ProtectedVideoType> {
  static viz::mojom::ProtectedVideoState ToMojom(
      gfx::ProtectedVideoType input) {
    switch (input) {
      case gfx::ProtectedVideoType::kClear:
        return viz::mojom::ProtectedVideoState::kClear;
      case gfx::ProtectedVideoType::kHardwareProtected:
        return viz::mojom::ProtectedVideoState::kHardwareProtected;
      case gfx::ProtectedVideoType::kSoftwareProtected:
        return viz::mojom::ProtectedVideoState::kSoftwareProtected;
    }
    NOTREACHED_IN_MIGRATION();
    return viz::mojom::ProtectedVideoState::kClear;
  }

  static bool FromMojom(viz::mojom::ProtectedVideoState input,
                        gfx::ProtectedVideoType* out) {
    switch (input) {
      case viz::mojom::ProtectedVideoState::kClear:
        *out = gfx::ProtectedVideoType::kClear;
        return true;
      case viz::mojom::ProtectedVideoState::kHardwareProtected:
        *out = gfx::ProtectedVideoType::kHardwareProtected;
        return true;
      case viz::mojom::ProtectedVideoState::kSoftwareProtected:
        *out = gfx::ProtectedVideoType::kSoftwareProtected;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<viz::mojom::OverlayPriority, viz::OverlayPriority> {
  static viz::mojom::OverlayPriority ToMojom(viz::OverlayPriority input) {
    switch (input) {
      case viz::OverlayPriority::kLow:
        return viz::mojom::OverlayPriority::kLow;
      case viz::OverlayPriority::kRegular:
        return viz::mojom::OverlayPriority::kRegular;
      case viz::OverlayPriority::kRequired:
        return viz::mojom::OverlayPriority::kRequired;
    }
    NOTREACHED_IN_MIGRATION();
    return viz::mojom::OverlayPriority::kLow;
  }

  static bool FromMojom(viz::mojom::OverlayPriority input,
                        viz::OverlayPriority* out) {
    switch (input) {
      case viz::mojom::OverlayPriority::kLow:
        *out = viz::OverlayPriority::kLow;
        return true;
      case viz::mojom::OverlayPriority::kRegular:
        *out = viz::OverlayPriority::kRegular;
        return true;
      case viz::mojom::OverlayPriority::kRequired:
        *out = viz::OverlayPriority::kRequired;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<viz::mojom::RoundedDisplayMasksInfoDataView,
                    viz::TextureDrawQuad::RoundedDisplayMasksInfo> {
  static bool is_horizontally_positioned(
      const viz::TextureDrawQuad::RoundedDisplayMasksInfo& input) {
    return input.is_horizontally_positioned;
  }

  static base::span<const uint8_t> radii(
      const viz::TextureDrawQuad::RoundedDisplayMasksInfo& input) {
    return input.radii;
  }

  static bool Read(viz::mojom::RoundedDisplayMasksInfoDataView data,
                   viz::TextureDrawQuad::RoundedDisplayMasksInfo* out);
};

template <>
struct UnionTraits<viz::mojom::DrawQuadStateDataView, viz::DrawQuad> {
  static viz::mojom::DrawQuadStateDataView::Tag GetTag(
      const viz::DrawQuad& quad) {
    switch (quad.material) {
      case viz::DrawQuad::Material::kInvalid:
        break;
      case viz::DrawQuad::Material::kAggregatedRenderPass:
        break;
      case viz::DrawQuad::Material::kDebugBorder:
        return viz::mojom::DrawQuadStateDataView::Tag::kDebugBorderQuadState;
      case viz::DrawQuad::Material::kPictureContent:
        break;
      case viz::DrawQuad::Material::kCompositorRenderPass:
        return viz::mojom::DrawQuadStateDataView::Tag::kRenderPassQuadState;
      case viz::DrawQuad::Material::kSolidColor:
        return viz::mojom::DrawQuadStateDataView::Tag::kSolidColorQuadState;
      case viz::DrawQuad::Material::kSurfaceContent:
        return viz::mojom::DrawQuadStateDataView::Tag::kSurfaceQuadState;
      case viz::DrawQuad::Material::kTextureContent:
        return viz::mojom::DrawQuadStateDataView::Tag::kTextureQuadState;
      case viz::DrawQuad::Material::kTiledContent:
        return viz::mojom::DrawQuadStateDataView::Tag::kTileQuadState;
      case viz::DrawQuad::Material::kVideoHole:
        return viz::mojom::DrawQuadStateDataView::Tag::kVideoHoleQuadState;
      case viz::DrawQuad::Material::kSharedElement:
        return viz::mojom::DrawQuadStateDataView::Tag::kSharedElementQuadState;
    }
    NOTREACHED_IN_MIGRATION();
    return viz::mojom::DrawQuadStateDataView::Tag::kDebugBorderQuadState;
  }

  static const viz::DrawQuad& debug_border_quad_state(
      const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& render_pass_quad_state(
      const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& solid_color_quad_state(
      const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& surface_quad_state(const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& texture_quad_state(const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& tile_quad_state(const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& stream_video_quad_state(
      const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& video_hole_quad_state(const viz::DrawQuad& quad) {
    return quad;
  }

  static const viz::DrawQuad& shared_element_quad_state(
      const viz::DrawQuad& quad) {
    return quad;
  }

  static bool Read(viz::mojom::DrawQuadStateDataView data, viz::DrawQuad* out) {
    switch (data.tag()) {
      case viz::mojom::DrawQuadStateDataView::Tag::kDebugBorderQuadState:
        return data.ReadDebugBorderQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kRenderPassQuadState:
        return data.ReadRenderPassQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kSolidColorQuadState:
        return data.ReadSolidColorQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kSurfaceQuadState:
        return data.ReadSurfaceQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kTextureQuadState:
        return data.ReadTextureQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kTileQuadState:
        return data.ReadTileQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kVideoHoleQuadState:
        return data.ReadVideoHoleQuadState(out);
      case viz::mojom::DrawQuadStateDataView::Tag::kSharedElementQuadState:
        return data.ReadSharedElementQuadState(out);
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<viz::mojom::SharedElementQuadStateDataView, viz::DrawQuad> {
  static const viz::ViewTransitionElementResourceId& resource_id(
      const viz::DrawQuad& input) {
    const viz::SharedElementDrawQuad* quad =
        viz::SharedElementDrawQuad::MaterialCast(&input);
    return quad->resource_id;
  }

  static bool Read(viz::mojom::SharedElementQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::VideoHoleQuadStateDataView, viz::DrawQuad> {
  static const base::UnguessableToken& overlay_plane_id(
      const viz::DrawQuad& input) {
    const viz::VideoHoleDrawQuad* quad =
        viz::VideoHoleDrawQuad::MaterialCast(&input);
    return quad->overlay_plane_id;
  }

  static bool Read(viz::mojom::VideoHoleQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::DebugBorderQuadStateDataView, viz::DrawQuad> {
  static SkColor4f color(const viz::DrawQuad& input) {
    const viz::DebugBorderDrawQuad* quad =
        viz::DebugBorderDrawQuad::MaterialCast(&input);
    return quad->color;
  }

  static int32_t width(const viz::DrawQuad& input) {
    const viz::DebugBorderDrawQuad* quad =
        viz::DebugBorderDrawQuad::MaterialCast(&input);
    return quad->width;
  }

  static bool Read(viz::mojom::DebugBorderQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::CompositorRenderPassQuadStateDataView,
                    viz::DrawQuad> {
  static viz::CompositorRenderPassId render_pass_id(
      const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    DCHECK(quad->render_pass_id);
    return quad->render_pass_id;
  }

  static viz::ResourceId mask_resource_id(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_resource_id();
  }

  static const gfx::RectF& mask_uv_rect(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_uv_rect;
  }

  static const gfx::Size& mask_texture_size(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_texture_size;
  }

  static const gfx::Vector2dF& filters_scale(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->filters_scale;
  }

  static const gfx::PointF& filters_origin(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->filters_origin;
  }

  static const gfx::RectF& tex_coord_rect(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->tex_coord_rect;
  }

  static bool force_anti_aliasing_off(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->force_anti_aliasing_off;
  }

  static float backdrop_filter_quality(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->backdrop_filter_quality;
  }

  static bool intersects_damage_under(const viz::DrawQuad& input) {
    const viz::CompositorRenderPassDrawQuad* quad =
        viz::CompositorRenderPassDrawQuad::MaterialCast(&input);
    return quad->intersects_damage_under;
  }

  static bool Read(viz::mojom::CompositorRenderPassQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::SolidColorQuadStateDataView, viz::DrawQuad> {
  static SkColor4f color(const viz::DrawQuad& input) {
    const viz::SolidColorDrawQuad* quad =
        viz::SolidColorDrawQuad::MaterialCast(&input);
    return quad->color;
  }

  static bool force_anti_aliasing_off(const viz::DrawQuad& input) {
    const viz::SolidColorDrawQuad* quad =
        viz::SolidColorDrawQuad::MaterialCast(&input);
    return quad->force_anti_aliasing_off;
  }

  static bool Read(viz::mojom::SolidColorQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::SurfaceQuadStateDataView, viz::DrawQuad> {
  static const viz::SurfaceRange& surface_range(const viz::DrawQuad& input) {
    const viz::SurfaceDrawQuad* quad =
        viz::SurfaceDrawQuad::MaterialCast(&input);
    return quad->surface_range;
  }

  static const SkColor4f default_background_color(const viz::DrawQuad& input) {
    const viz::SurfaceDrawQuad* quad =
        viz::SurfaceDrawQuad::MaterialCast(&input);
    return quad->default_background_color;
  }

  static bool stretch_content_to_fill_bounds(const viz::DrawQuad& input) {
    const viz::SurfaceDrawQuad* quad =
        viz::SurfaceDrawQuad::MaterialCast(&input);
    return quad->stretch_content_to_fill_bounds;
  }

  static bool is_reflection(const viz::DrawQuad& input) {
    const viz::SurfaceDrawQuad* quad =
        viz::SurfaceDrawQuad::MaterialCast(&input);
    return quad->is_reflection;
  }

  static bool allow_merge(const viz::DrawQuad& input) {
    const viz::SurfaceDrawQuad* quad =
        viz::SurfaceDrawQuad::MaterialCast(&input);
    return quad->allow_merge;
  }

  static bool Read(viz::mojom::SurfaceQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::TextureQuadStateDataView, viz::DrawQuad> {
  static viz::ResourceId resource_id(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->resource_id();
  }

  static const gfx::Size& resource_size_in_pixels(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->resource_size_in_pixels();
  }

  static viz::TextureDrawQuad::RoundedDisplayMasksInfo
  rounded_display_masks_info(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->rounded_display_masks_info;
  }

  static bool premultiplied_alpha(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->premultiplied_alpha;
  }

  static const gfx::PointF& uv_top_left(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->uv_top_left;
  }

  static const gfx::PointF& uv_bottom_right(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->uv_bottom_right;
  }

  static SkColor4f background_color(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->background_color;
  }

  static bool y_flipped(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->y_flipped;
  }

  static bool force_rgbx(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->force_rgbx;
  }

  static bool nearest_neighbor(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->nearest_neighbor;
  }

  static bool secure_output_only(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->secure_output_only;
  }

  static bool is_stream_video(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->is_stream_video;
  }

  static bool is_video_frame(const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->is_video_frame;
  }

  static gfx::ProtectedVideoType protected_video_type(
      const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->protected_video_type;
  }

  static viz::OverlayPriority overlay_priority_hint(
      const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->overlay_priority_hint;
  }

  static const std::optional<gfx::Rect>& damage_rect(
      const viz::DrawQuad& input) {
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(&input);
    return quad->damage_rect;
  }

  static bool Read(viz::mojom::TextureQuadStateDataView data,
                   viz::DrawQuad* out);
};

template <>
struct StructTraits<viz::mojom::TileQuadStateDataView, viz::DrawQuad> {
  static const gfx::RectF& tex_coord_rect(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->tex_coord_rect;
  }

  static const gfx::Size& texture_size(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->texture_size;
  }

  static bool is_premultiplied(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->is_premultiplied;
  }

  static bool nearest_neighbor(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->nearest_neighbor;
  }

  static viz::ResourceId resource_id(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->resource_id();
  }

  static bool force_anti_aliasing_off(const viz::DrawQuad& input) {
    const viz::TileDrawQuad* quad = viz::TileDrawQuad::MaterialCast(&input);
    return quad->force_anti_aliasing_off;
  }

  static bool Read(viz::mojom::TileQuadStateDataView data, viz::DrawQuad* out);
};

struct DrawQuadWithSharedQuadState {
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION const viz::DrawQuad* quad = nullptr;
  RAW_PTR_EXCLUSION const viz::SharedQuadState* shared_quad_state = nullptr;
};

template <>
struct StructTraits<viz::mojom::DrawQuadDataView, DrawQuadWithSharedQuadState> {
  static const gfx::Rect& rect(const DrawQuadWithSharedQuadState& input) {
    return input.quad->rect;
  }

  static const gfx::Rect& visible_rect(
      const DrawQuadWithSharedQuadState& input) {
    return input.quad->visible_rect;
  }

  static bool needs_blending(const DrawQuadWithSharedQuadState& input) {
    return input.quad->needs_blending;
  }

  static OptSharedQuadState sqs(const DrawQuadWithSharedQuadState& input) {
    return {input.shared_quad_state};
  }

  static const viz::DrawQuad& draw_quad_state(
      const DrawQuadWithSharedQuadState& input) {
    return *input.quad;
  }
};

// This StructTraits is only used for deserialization within
// CompositorRenderPasses.
template <>
struct StructTraits<viz::mojom::DrawQuadDataView, viz::DrawQuad> {
  static bool Read(viz::mojom::DrawQuadDataView data, viz::DrawQuad* out);
};

template <>
struct ArrayTraits<viz::QuadList> {
  using Element = DrawQuadWithSharedQuadState;
  struct ConstIterator {
    STACK_ALLOCATED();

   public:
    explicit ConstIterator(const viz::QuadList::ConstIterator& it)
        : it(it), last_shared_quad_state(nullptr) {}

    viz::QuadList::ConstIterator it;
    const viz::SharedQuadState* last_shared_quad_state = nullptr;
  };

  static ConstIterator GetBegin(const viz::QuadList& input) {
    return ConstIterator(input.begin());
  }

  static void AdvanceIterator(ConstIterator& iterator) {  // NOLINT
    iterator.last_shared_quad_state = (*iterator.it)->shared_quad_state;
    ++iterator.it;
  }

  static Element GetValue(ConstIterator& iterator) {  // NOLINT
    DrawQuadWithSharedQuadState dq = {*iterator.it, nullptr};
    // Only serialize the SharedQuadState if we haven't seen it before and
    // therefore have not already serialized it.
    const viz::SharedQuadState* current_sqs = (*iterator.it)->shared_quad_state;
    if (current_sqs != iterator.last_shared_quad_state)
      dq.shared_quad_state = current_sqs;
    return dq;
  }

  static size_t GetSize(const viz::QuadList& input) { return input.size(); }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_QUADS_MOJOM_TRAITS_H_
