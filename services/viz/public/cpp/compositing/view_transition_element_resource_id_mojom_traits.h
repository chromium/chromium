// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_

#include "services/viz/public/mojom/compositing/view_transition_element_resource_id.mojom-shared.h"

#include "components/viz/common/view_transition_element_resource_id.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::ViewTransitionElementResourceIdDataView,
                    viz::ViewTransitionElementResourceId> {
  static uint32_t local_id(
      const viz::ViewTransitionElementResourceId& resource_id) {
    return resource_id.local_id();
  }

  static const base::UnguessableToken& transition_id(
      const viz::ViewTransitionElementResourceId& resource_id) {
    return resource_id.transition_id();
  }

  static bool Read(viz::mojom::ViewTransitionElementResourceIdDataView data,
                   viz::ViewTransitionElementResourceId* out) {
    base::UnguessableToken transition_id;
    if (!data.ReadTransitionId(&transition_id)) {
      return false;
    }
    *out = viz::ViewTransitionElementResourceId(transition_id, data.local_id());
    return out->IsValid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
