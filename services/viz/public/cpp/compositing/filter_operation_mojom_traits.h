// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_

#include <optional>

#include "base/containers/span.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/paint_filter.h"
#include "services/viz/public/cpp/compositing/paint_filter_mojom_traits.h"
#include "services/viz/public/mojom/compositing/filter_operation.mojom-shared.h"
#include "skia/public/mojom/skcolor4f_mojom_traits.h"
#include "skia/public/mojom/tile_mode_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FilterOperationDataView, cc::FilterOperation> {
  static viz::mojom::FilterType type(const cc::FilterOperation& op);

  static float amount(const cc::FilterOperation& operation) {
    if (operation.type() == cc::FilterOperation::ALPHA_THRESHOLD ||
        operation.type() == cc::FilterOperation::COLOR_MATRIX ||
        operation.type() == cc::FilterOperation::REFERENCE) {
      return 0.f;
    }
    return operation.amount();
  }

  static gfx::Point offset(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW &&
        operation.type() != cc::FilterOperation::OFFSET)
      return gfx::Point();
    return operation.offset();
  }

  static SkColor4f drop_shadow_color(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW)
      return SkColors::kTransparent;
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

  static std::optional<base::span<const float>> matrix(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::COLOR_MATRIX)
      return std::nullopt;
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

  static SkTileMode blur_tile_mode(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::BLUR)
      return SkTileMode::kDecal;
    return operation.blur_tile_mode();
  }

  static bool Read(viz::mojom::FilterOperationDataView data,
                   cc::FilterOperation* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FILTER_OPERATION_MOJOM_TRAITS_H_
