// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/optional.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/paint_filter.h"
#include "services/viz/public/cpp/compositing/paint_filter_mojom_traits.h"
#include "services/viz/public/mojom/compositing/filter_operation.mojom-shared.h"
#include "skia/public/mojom/blur_image_filter_tile_mode_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

namespace {
viz::mojom::FilterType CCFilterTypeToMojo(
    const cc::FilterOperation::FilterType& type) {
  switch (type) {
    case cc::FilterOperation::GRAYSCALE:
      return viz::mojom::FilterType::GRAYSCALE;
    case cc::FilterOperation::SEPIA:
      return viz::mojom::FilterType::SEPIA;
    case cc::FilterOperation::SATURATE:
      return viz::mojom::FilterType::SATURATE;
    case cc::FilterOperation::HUE_ROTATE:
      return viz::mojom::FilterType::HUE_ROTATE;
    case cc::FilterOperation::INVERT:
      return viz::mojom::FilterType::INVERT;
    case cc::FilterOperation::BRIGHTNESS:
      return viz::mojom::FilterType::BRIGHTNESS;
    case cc::FilterOperation::CONTRAST:
      return viz::mojom::FilterType::CONTRAST;
    case cc::FilterOperation::OPACITY:
      return viz::mojom::FilterType::OPACITY;
    case cc::FilterOperation::BLUR:
      return viz::mojom::FilterType::BLUR;
    case cc::FilterOperation::DROP_SHADOW:
      return viz::mojom::FilterType::DROP_SHADOW;
    case cc::FilterOperation::COLOR_MATRIX:
      return viz::mojom::FilterType::COLOR_MATRIX;
    case cc::FilterOperation::ZOOM:
      return viz::mojom::FilterType::ZOOM;
    case cc::FilterOperation::REFERENCE:
      return viz::mojom::FilterType::REFERENCE;
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
      return viz::mojom::FilterType::SATURATING_BRIGHTNESS;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      return viz::mojom::FilterType::ALPHA_THRESHOLD;
  }
  NOTREACHED();
  return viz::mojom::FilterType::FILTER_TYPE_LAST;
}

cc::FilterOperation::FilterType MojoFilterTypeToCC(
    const viz::mojom::FilterType& type) {
  switch (type) {
    case viz::mojom::FilterType::GRAYSCALE:
      return cc::FilterOperation::GRAYSCALE;
    case viz::mojom::FilterType::SEPIA:
      return cc::FilterOperation::SEPIA;
    case viz::mojom::FilterType::SATURATE:
      return cc::FilterOperation::SATURATE;
    case viz::mojom::FilterType::HUE_ROTATE:
      return cc::FilterOperation::HUE_ROTATE;
    case viz::mojom::FilterType::INVERT:
      return cc::FilterOperation::INVERT;
    case viz::mojom::FilterType::BRIGHTNESS:
      return cc::FilterOperation::BRIGHTNESS;
    case viz::mojom::FilterType::CONTRAST:
      return cc::FilterOperation::CONTRAST;
    case viz::mojom::FilterType::OPACITY:
      return cc::FilterOperation::OPACITY;
    case viz::mojom::FilterType::BLUR:
      return cc::FilterOperation::BLUR;
    case viz::mojom::FilterType::DROP_SHADOW:
      return cc::FilterOperation::DROP_SHADOW;
    case viz::mojom::FilterType::COLOR_MATRIX:
      return cc::FilterOperation::COLOR_MATRIX;
    case viz::mojom::FilterType::ZOOM:
      return cc::FilterOperation::ZOOM;
    case viz::mojom::FilterType::REFERENCE:
      return cc::FilterOperation::REFERENCE;
    case viz::mojom::FilterType::SATURATING_BRIGHTNESS:
      return cc::FilterOperation::SATURATING_BRIGHTNESS;
    case viz::mojom::FilterType::ALPHA_THRESHOLD:
      return cc::FilterOperation::ALPHA_THRESHOLD;
  }
  NOTREACHED();
  return cc::FilterOperation::FILTER_TYPE_LAST;
}

}  // namespace

template <>
struct StructTraits<viz::mojom::FilterOperationDataView, cc::FilterOperation> {
  static viz::mojom::FilterType type(const cc::FilterOperation& op) {
    return CCFilterTypeToMojo(op.type());
  }

  static float amount(const cc::FilterOperation& operation) {
    if (operation.type() == cc::FilterOperation::COLOR_MATRIX ||
        operation.type() == cc::FilterOperation::REFERENCE) {
      return 0.f;
    }
    return operation.amount();
  }

  static float outer_threshold(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::ALPHA_THRESHOLD)
      return 0.f;
    return operation.outer_threshold();
  }

  static gfx::Point drop_shadow_offset(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW)
      return gfx::Point();
    return operation.drop_shadow_offset();
  }

  static uint32_t drop_shadow_color(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW)
      return 0;
    return operation.drop_shadow_color();
  }

  static sk_sp<cc::PaintFilter> image_filter(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::REFERENCE)
      return nullptr;
    if (!operation.image_filter())
      return nullptr;
    return operation.image_filter();
  }

  static base::Optional<base::span<const float>> matrix(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::COLOR_MATRIX)
      return base::nullopt;
    return base::make_span(operation.matrix());
  }

  static base::span<const gfx::Rect> shape(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::ALPHA_THRESHOLD)
      return base::span<gfx::Rect>();
    return operation.shape();
  }

  static int32_t zoom_inset(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::ZOOM)
      return 0;
    return operation.zoom_inset();
  }

  static skia::mojom::BlurTileMode blur_tile_mode(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::BLUR)
      return skia::mojom::BlurTileMode::CLAMP_TO_BLACK;
    return EnumTraits<skia::mojom::BlurTileMode, SkBlurImageFilter::TileMode>::
        ToMojom(operation.blur_tile_mode());
  }

  static bool Read(viz::mojom::FilterOperationDataView data,
                   cc::FilterOperation* out) {
    out->set_type(MojoFilterTypeToCC(data.type()));
    switch (out->type()) {
      case cc::FilterOperation::GRAYSCALE:
      case cc::FilterOperation::SEPIA:
      case cc::FilterOperation::SATURATE:
      case cc::FilterOperation::HUE_ROTATE:
      case cc::FilterOperation::INVERT:
      case cc::FilterOperation::BRIGHTNESS:
      case cc::FilterOperation::SATURATING_BRIGHTNESS:
      case cc::FilterOperation::CONTRAST:
      case cc::FilterOperation::OPACITY:
        out->set_amount(data.amount());
        return true;
      case cc::FilterOperation::BLUR:
        out->set_amount(data.amount());
        SkBlurImageFilter::TileMode tile_mode;
        if (!data.ReadBlurTileMode(&tile_mode))
          return false;
        out->set_blur_tile_mode(tile_mode);
        return true;
      case cc::FilterOperation::DROP_SHADOW: {
        out->set_amount(data.amount());
        gfx::Point offset;
        if (!data.ReadDropShadowOffset(&offset))
          return false;
        out->set_drop_shadow_offset(offset);
        out->set_drop_shadow_color(data.drop_shadow_color());
        return true;
      }
      case cc::FilterOperation::COLOR_MATRIX: {
        // TODO(fsamuel): It would be nice to modify cc::FilterOperation to
        // avoid this extra copy.
        cc::FilterOperation::Matrix matrix_buffer = {};
        base::span<float> matrix(matrix_buffer);
        if (!data.ReadMatrix(&matrix))
          return false;
        out->set_matrix(matrix_buffer);
        return true;
      }
      case cc::FilterOperation::ZOOM: {
        if (data.amount() < 0.f || data.zoom_inset() < 0)
          return false;
        out->set_amount(data.amount());
        out->set_zoom_inset(data.zoom_inset());
        return true;
      }
      case cc::FilterOperation::REFERENCE: {
        sk_sp<cc::PaintFilter> filter;
        if (!data.ReadImageFilter(&filter))
          return false;
        out->set_image_filter(std::move(filter));
        return true;
      }
      case cc::FilterOperation::ALPHA_THRESHOLD:
        out->set_amount(data.amount());
        out->set_outer_threshold(data.outer_threshold());
        cc::FilterOperation::ShapeRects shape;
        if (!data.ReadShape(&shape))
          return false;
        out->set_shape(shape);
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_
