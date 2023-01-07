// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_ID_MOJOM_TRAITS_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "services/viz/public/cpp/compositing/frame_sink_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/local_surface_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/surface_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SurfaceIdDataView, viz::SurfaceId> {
  static const viz::FrameSinkId& frame_sink_id(const viz::SurfaceId& id) {
    return id.frame_sink_id();
  }

  static const viz::LocalSurfaceId& local_surface_id(const viz::SurfaceId& id) {
    return id.local_surface_id();
  }

  static bool Read(viz::mojom::SurfaceIdDataView data, viz::SurfaceId* out) {
    return data.ReadFrameSinkId(&out->frame_sink_id_) &&
           data.ReadLocalSurfaceId(&out->local_surface_id_);
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SURFACE_ID_MOJOM_TRAITS_H_
