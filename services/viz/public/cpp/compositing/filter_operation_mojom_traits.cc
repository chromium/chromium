// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/filter_operation_mojom_traits.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"

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
    case cc::FilterOperation::OFFSET:
      return viz::mojom::FilterType::OFFSET;
  }
  NOTREACHED_IN_MIGRATION();
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
    case viz::mojom::FilterType::OFFSET:
      return cc::FilterOperation::OFFSET;
  }
  NOTREACHED_IN_MIGRATION();
  return cc::FilterOperation::FILTER_TYPE_LAST;
}

}  // namespace

// static
viz::mojom::FilterType
StructTraits<viz::mojom::FilterOperationDataView, cc::FilterOperation>::type(
    const cc::FilterOperation& op) {
  return CCFilterTypeToMojo(op.type());
}

// static
bool StructTraits<viz::mojom::FilterOperationDataView, cc::FilterOperation>::
    Read(viz::mojom::FilterOperationDataView data, cc::FilterOperation* out) {
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
      SkTileMode tile_mode;
      if (!data.ReadBlurTileMode(&tile_mode))
        return false;
      out->set_blur_tile_mode(tile_mode);
      return true;
    case cc::FilterOperation::DROP_SHADOW: {
      out->set_amount(data.amount());
      gfx::Point offset;
      SkColor4f drop_shadow_color;
      if (!data.ReadOffset(&offset) ||
          !data.ReadDropShadowColor(&drop_shadow_color))
        return false;
      out->set_offset(offset);
      out->set_drop_shadow_color(drop_shadow_color);
      return true;
    }
    case cc::FilterOperation::COLOR_MATRIX: {
      mojo::ArrayDataView<float> matrix;
      data.GetMatrixDataView(&matrix);
      if (!matrix.is_null()) {
        // Guaranteed by prior validation of the FilterOperation struct
        // because this array specifies a fixed size in the mojom.
        out->set_matrix(*base::span(matrix).to_fixed_extent<20>());
      }
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
    case cc::FilterOperation::ALPHA_THRESHOLD: {
      cc::FilterOperation::ShapeRects shape;
      if (!data.ReadShape(&shape))
        return false;
      out->set_shape(shape);
      return true;
    }
    case cc::FilterOperation::OFFSET: {
      gfx::Point offset;
      if (!data.ReadOffset(&offset))
        return false;
      out->set_offset(offset);
      return true;
    }
  }
  return false;
}

}  // namespace mojo
