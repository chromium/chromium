// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_

#include <optional>

#include "base/types/expected.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "mojo/public/cpp/bindings/deserialization_error.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"
#include "services/viz/public/mojom/compositing/shared_quad_state.mojom-shared.h"
#include "skia/public/mojom/blend_mode_mojom_traits.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/mojom/mask_filter_info_mojom_traits.h"

namespace mojo {

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

  static mojo::OptionalAsPointer<const gfx::MaskFilterInfo> mask_filter_info(
      const viz::SharedQuadState& sqs) {
    return sqs.mask_filter_info.IsEmpty()
               ? nullptr
               : mojo::OptionalAsPointer(&sqs.mask_filter_info);
  }

  static const std::optional<gfx::Rect>& clip_rect(
      const viz::SharedQuadState& sqs) {
    return sqs.clip_rect;
  }

  static bool are_contents_opaque(const viz::SharedQuadState& sqs) {
    return sqs.are_contents_opaque;
  }

  static float opacity(const viz::SharedQuadState& sqs) { return sqs.opacity; }

  static SkBlendMode blend_mode(const viz::SharedQuadState& sqs) {
    return sqs.blend_mode;
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

  static base::expected<void, DeserializationError> Read(
      viz::mojom::SharedQuadStateDataView data,
      viz::SharedQuadState* out) {
    if (!data.ReadQuadToTargetTransform(&out->quad_to_target_transform)) {
      return base::unexpected(DeserializationError());
    }
    if (!data.ReadQuadLayerRect(&out->quad_layer_rect)) {
      return base::unexpected(DeserializationError());
    }
    if (!data.ReadVisibleQuadLayerRect(&out->visible_quad_layer_rect)) {
      return base::unexpected(DeserializationError());
    }
    if (!data.ReadClipRect(&out->clip_rect)) {
      return base::unexpected(DeserializationError());
    }
    if (!data.ReadOffsetTag(&out->offset_tag)) {
      return base::unexpected(DeserializationError());
    }

    std::optional<gfx::MaskFilterInfo> mask_filter;
    if (!data.ReadMaskFilterInfo(&mask_filter)) {
      return base::unexpected(DeserializationError());
    }

    out->mask_filter_info = mask_filter.value_or(gfx::MaskFilterInfo());

    out->are_contents_opaque = data.are_contents_opaque();
    out->opacity = data.opacity();
    if (!data.ReadBlendMode(&out->blend_mode)) {
      return base::unexpected(DeserializationError());
    }
    out->sorting_context_id = data.sorting_context_id();
    out->layer_id = data.layer_id();
    out->is_fast_rounded_corner = data.is_fast_rounded_corner();

    return base::ok();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_QUAD_STATE_MOJOM_TRAITS_H_
