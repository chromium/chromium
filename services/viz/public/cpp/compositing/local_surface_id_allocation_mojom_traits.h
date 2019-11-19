// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_LOCAL_SURFACE_ID_ALLOCATION_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_LOCAL_SURFACE_ID_ALLOCATION_MOJOM_TRAITS_H_

#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/viz/public/cpp/compositing/local_surface_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/local_surface_id_allocation.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::LocalSurfaceIdAllocationDataView,
                    viz::LocalSurfaceIdAllocation> {
  static const viz::LocalSurfaceId& local_surface_id(
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
    DCHECK(local_surface_id_allocation.IsValid());
    return local_surface_id_allocation.local_surface_id();
  }

  static base::TimeTicks allocation_time(
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
    DCHECK(local_surface_id_allocation.IsValid());
    return local_surface_id_allocation.allocation_time();
  }

  static bool Read(viz::mojom::LocalSurfaceIdAllocationDataView data,
                   viz::LocalSurfaceIdAllocation* out) {
    return data.ReadLocalSurfaceId(&out->local_surface_id_) &&
           data.ReadAllocationTime(&out->allocation_time_) && out->IsValid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_LOCAL_SURFACE_ID_ALLOCATION_MOJOM_TRAITS_H_
