// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_

#include <optional>

#include "base/check_op.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"
#include "services/viz/public/mojom/compositing/shared_quad_state.mojom-shared.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/mojom/mask_filter_info_mojom_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"

namespace mojo {

struct OptSharedQuadState {
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION const viz::SharedQuadState* sqs = nullptr;
};

template <>
struct StructTraits<viz::mojom::SharedQuadStateDataView, OptSharedQuadState> {
  static bool IsNull(const OptSharedQuadState& input) { return !input.sqs; }

  static void SetToNull(OptSharedQuadState* output) { output->sqs = nullptr; }

  static const gfx::Transform& quad_to_target_transform(
      const OptSharedQuadState& input) {
    return input.sqs->quad_to_target_transform;
  }

  static const gfx::Rect& quad_layer_rect(const OptSharedQuadState& input) {
    return input.sqs->quad_layer_rect;
  }

  static const gfx::Rect& visible_quad_layer_rect(
      const OptSharedQuadState& input) {
    return input.sqs->visible_quad_layer_rect;
  }

  static const std::optional<gfx::MaskFilterInfo> mask_filter_info(
      const OptSharedQuadState& input) {
    return input.sqs->mask_filter_info.IsEmpty()
               ? std::nullopt
               : std::optional<gfx::MaskFilterInfo>(
                     input.sqs->mask_filter_info);
  }

  static const std::optional<gfx::Rect>& clip_rect(
      const OptSharedQuadState& input) {
    return input.sqs->clip_rect;
  }

  static bool are_contents_opaque(const OptSharedQuadState& input) {
    return input.sqs->are_contents_opaque;
  }

  static float opacity(const OptSharedQuadState& input) {
    return input.sqs->opacity;
  }

  static uint32_t blend_mode(const OptSharedQuadState& input) {
    return static_cast<uint32_t>(input.sqs->blend_mode);
  }

  static int32_t sorting_context_id(const OptSharedQuadState& input) {
    return input.sqs->sorting_context_id;
  }

  static uint32_t layer_id(const OptSharedQuadState& input) {
    return input.sqs->layer_id;
  }

  static bool is_fast_rounded_corner(const OptSharedQuadState& input) {
    return input.sqs->is_fast_rounded_corner;
  }

  static const viz::OffsetTag& offset_tag(const OptSharedQuadState& input) {
    return input.sqs->offset_tag;
  }
};

template <>
struct StructTraits<viz::mojom::SharedQuadStateDataView, viz::SharedQuadState> {
  static const gfx::Transform& quad_to_target_transform(
      const viz::SharedQuadState& sqs) {
    return sqs.quad_to_target_transform;
  }

  static const gfx::Rect& quad_layer_rect(const viz::SharedQuadState& sqs) {
    return sqs.quad_layer_rect;
  }

  static const gfx::Rect& visible_quad_layer_rect(
      const viz::SharedQuadState& sqs) {
    return sqs.visible_quad_layer_rect;
  }

  static const gfx::MaskFilterInfo& mask_filter_info(
      const viz::SharedQuadState& sqs) {
    return sqs.mask_filter_info;
  }

  static const std::optional<gfx::Rect>& clip_rect(
      const viz::SharedQuadState& sqs) {
    return sqs.clip_rect;
  }

  static bool are_contents_opaque(const viz::SharedQuadState& sqs) {
    return sqs.are_contents_opaque;
  }

  static float opacity(const viz::SharedQuadState& sqs) { return sqs.opacity; }

  static uint32_t blend_mode(const viz::SharedQuadState& sqs) {
    return static_cast<uint32_t>(sqs.blend_mode);
  }

  static int32_t sorting_context_id(const viz::SharedQuadState& sqs) {
    return sqs.sorting_context_id;
  }

  static uint32_t layer_id(const viz::SharedQuadState& sqs) {
    return sqs.layer_id;
  }

  static bool is_fast_rounded_corner(const viz::SharedQuadState& sqs) {
    return sqs.is_fast_rounded_corner;
  }

  static const viz::OffsetTag& offset_tag(const viz::SharedQuadState& sqs) {
    return sqs.offset_tag;
  }

  static bool Read(viz::mojom::SharedQuadStateDataView data,
                   viz::SharedQuadState* out) {
    if (!data.ReadQuadToTargetTransform(&out->quad_to_target_transform) ||
        !data.ReadQuadLayerRect(&out->quad_layer_rect) ||
        !data.ReadVisibleQuadLayerRect(&out->visible_quad_layer_rect) ||
        !data.ReadClipRect(&out->clip_rect) ||
        !data.ReadOffsetTag(&out->offset_tag)) {
      return false;
    }

    std::optional<gfx::MaskFilterInfo> mask_filter;
    if (!data.ReadMaskFilterInfo(&mask_filter)) {
      return false;
    }

    out->mask_filter_info = mask_filter.value_or(gfx::MaskFilterInfo());

    out->are_contents_opaque = data.are_contents_opaque();
    out->opacity = data.opacity();
    if (data.blend_mode() > static_cast<int>(SkBlendMode::kLastMode)) {
      return false;
    }
    out->blend_mode = static_cast<SkBlendMode>(data.blend_mode());
    out->sorting_context_id = data.sorting_context_id();
    out->layer_id = data.layer_id();
    out->is_fast_rounded_corner = data.is_fast_rounded_corner();

    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_
