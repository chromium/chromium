// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_ID_MOJOM_TRAITS_H_

#include "components/viz/common/resources/resource_id.h"
#include "services/viz/public/mojom/compositing/resource_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::ResourceIdDataView, viz::ResourceId> {
  static uint32_t value(const viz::ResourceId& id);

  static bool Read(viz::mojom::ResourceIdDataView data, viz::ResourceId* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_ID_MOJOM_TRAITS_H_
