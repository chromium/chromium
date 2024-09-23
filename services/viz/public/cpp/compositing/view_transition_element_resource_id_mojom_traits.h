// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_

#include "components/viz/common/view_transition_element_resource_id.h"
#include "services/viz/public/mojom/compositing/view_transition_element_resource_id.mojom-shared.h"
#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::ViewTransitionElementResourceIdDataView,
                    viz::ViewTransitionElementResourceId> {
  static uint32_t local_id(
      const viz::ViewTransitionElementResourceId& resource_id) {
    return resource_id.local_id();
  }

  static std::optional<blink::ViewTransitionToken> transition_token(
      const viz::ViewTransitionElementResourceId& resource_id) {
    if (resource_id.IsValid()) {
      return {resource_id.transition_token()};
    }
    return std::nullopt;
  }

  static bool Read(viz::mojom::ViewTransitionElementResourceIdDataView data,
                   viz::ViewTransitionElementResourceId* out) {
    std::optional<blink::ViewTransitionToken> transition_token;
    if (!data.ReadTransitionToken(&transition_token)) {
      return false;
    }
    if (transition_token) {
      *out = viz::ViewTransitionElementResourceId(*transition_token,
                                                  data.local_id());
    } else {
      *out = viz::ViewTransitionElementResourceId();
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_MOJOM_TRAITS_H_
