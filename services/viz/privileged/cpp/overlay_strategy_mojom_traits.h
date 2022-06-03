// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PRIVILEGED_CPP_OVERLAY_STRATEGY_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PRIVILEGED_CPP_OVERLAY_STRATEGY_MOJOM_TRAITS_H_

#include "components/viz/common/display/overlay_strategy.h"
#include "services/viz/privileged/mojom/compositing/overlay_strategy.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::OverlayStrategy, viz::OverlayStrategy> {
  static viz::mojom::OverlayStrategy ToMojom(viz::OverlayStrategy reason) {
    switch (reason) {
      case viz::OverlayStrategy::kFullscreen:
        return viz::mojom::OverlayStrategy::kFullscreen;
      case viz::OverlayStrategy::kSingleOnTop:
        return viz::mojom::OverlayStrategy::kSingleOnTop;
      case viz::OverlayStrategy::kUnderlay:
        return viz::mojom::OverlayStrategy::kUnderlay;
      case viz::OverlayStrategy::kUnderlayCast:
        return viz::mojom::OverlayStrategy::kUnderlayCast;
      default:
        break;
    }
    NOTREACHED();
    return viz::mojom::OverlayStrategy::kFullscreen;
  }

  static bool FromMojom(viz::mojom::OverlayStrategy reason,
                        viz::OverlayStrategy* out) {
    switch (reason) {
      case viz::mojom::OverlayStrategy::kFullscreen:
        *out = viz::OverlayStrategy::kFullscreen;
        return true;
      case viz::mojom::OverlayStrategy::kSingleOnTop:
        *out = viz::OverlayStrategy::kSingleOnTop;
        return true;
      case viz::mojom::OverlayStrategy::kUnderlay:
        *out = viz::OverlayStrategy::kUnderlay;
        return true;
      case viz::mojom::OverlayStrategy::kUnderlayCast:
        *out = viz::OverlayStrategy::kUnderlayCast;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PRIVILEGED_CPP_OVERLAY_STRATEGY_MOJOM_TRAITS_H_
