// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_

#include "services/viz/public/mojom/compositing/shared_element_resource_id.mojom-shared.h"

#include "components/viz/common/shared_element_resource_id.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SharedElementResourceIdDataView,
                    viz::SharedElementResourceId> {
  static uint32_t id(const viz::SharedElementResourceId& resource_id) {
    return resource_id.id();
  }

  static bool Read(viz::mojom::SharedElementResourceIdDataView data,
                   viz::SharedElementResourceId* out) {
    *out = viz::SharedElementResourceId(data.id());
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
