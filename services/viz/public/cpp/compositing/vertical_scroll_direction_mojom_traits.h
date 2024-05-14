// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VERTICAL_SCROLL_DIRECTION_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VERTICAL_SCROLL_DIRECTION_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "services/viz/public/mojom/compositing/vertical_scroll_direction.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::VerticalScrollDirection,
                  viz::VerticalScrollDirection> {
  static viz::mojom::VerticalScrollDirection ToMojom(
      viz::VerticalScrollDirection in) {
    switch (in) {
      case viz::VerticalScrollDirection::kNull:
        return viz::mojom::VerticalScrollDirection::kNull;
      case viz::VerticalScrollDirection::kDown:
        return viz::mojom::VerticalScrollDirection::kDown;
      case viz::VerticalScrollDirection::kUp:
        return viz::mojom::VerticalScrollDirection::kUp;
    }
    NOTREACHED_IN_MIGRATION();
    return viz::mojom::VerticalScrollDirection::kNull;
  }

  static bool FromMojom(viz::mojom::VerticalScrollDirection in,
                        viz::VerticalScrollDirection* out) {
    switch (in) {
      case viz::mojom::VerticalScrollDirection::kNull:
        *out = viz::VerticalScrollDirection::kNull;
        return true;
      case viz::mojom::VerticalScrollDirection::kDown:
        *out = viz::VerticalScrollDirection::kDown;
        return true;
      case viz::mojom::VerticalScrollDirection::kUp:
        *out = viz::VerticalScrollDirection::kUp;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VERTICAL_SCROLL_DIRECTION_MOJOM_TRAITS_H_
