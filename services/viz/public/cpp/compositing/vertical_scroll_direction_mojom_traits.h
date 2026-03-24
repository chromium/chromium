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
    NOTREACHED();
  }

  static viz::VerticalScrollDirection FromMojom(
      viz::mojom::VerticalScrollDirection in) {
    switch (in) {
      case viz::mojom::VerticalScrollDirection::kNull:
        return viz::VerticalScrollDirection::kNull;
      case viz::mojom::VerticalScrollDirection::kDown:
        return viz::VerticalScrollDirection::kDown;
      case viz::mojom::VerticalScrollDirection::kUp:
        return viz::VerticalScrollDirection::kUp;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VERTICAL_SCROLL_DIRECTION_MOJOM_TRAITS_H_
