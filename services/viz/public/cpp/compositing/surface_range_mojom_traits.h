// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_RANGE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_RANGE_MOJOM_TRAITS_H_

#include <optional>

#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/surface_range.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SurfaceRangeDataView, viz::SurfaceRange> {
  static const std::optional<viz::SurfaceId>& start(
      const viz::SurfaceRange& range) {
    DCHECK(range.IsValid());
    return range.start();
  }

  static const viz::SurfaceId& end(const viz::SurfaceRange& range) {
    DCHECK(range.IsValid());
    return range.end();
  }

  static bool Read(viz::mojom::SurfaceRangeDataView data,
                   viz::SurfaceRange* out) {
    return data.ReadStart(&out->start_) && data.ReadEnd(&out->end_) &&
           out->IsValid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_RANGE_MOJOM_TRAITS_H_
