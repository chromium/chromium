// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_POINT_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_POINT_MOJOM_TRAITS_H_

#include "components/viz/common/delegated_ink_point.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/viz/public/mojom/compositing/delegated_ink_point.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::DelegatedInkPointDataView,
                    viz::DelegatedInkPoint> {
  static const gfx::PointF& point(const viz::DelegatedInkPoint& input) {
    return input.point();
  }

  static base::TimeTicks timestamp(const viz::DelegatedInkPoint& input) {
    return input.timestamp();
  }

  static int32_t pointer_id(const viz::DelegatedInkPoint& input) {
    return input.pointer_id();
  }

  static bool Read(viz::mojom::DelegatedInkPointDataView data,
                   viz::DelegatedInkPoint* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_POINT_MOJOM_TRAITS_H_
