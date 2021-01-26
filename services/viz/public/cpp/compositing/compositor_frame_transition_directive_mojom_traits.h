// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_

#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "services/viz/public/mojom/compositing/compositor_frame_transition_directive.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveType,
                  viz::CompositorFrameTransitionDirective::Type> {
  static viz::mojom::CompositorFrameTransitionDirectiveType ToMojom(
      viz::CompositorFrameTransitionDirective::Type type);

  static bool FromMojom(
      viz::mojom::CompositorFrameTransitionDirectiveType input,
      viz::CompositorFrameTransitionDirective::Type* out);
};

template <>
struct EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveEffect,
                  viz::CompositorFrameTransitionDirective::Effect> {
  static viz::mojom::CompositorFrameTransitionDirectiveEffect ToMojom(
      viz::CompositorFrameTransitionDirective::Effect type);

  static bool FromMojom(
      viz::mojom::CompositorFrameTransitionDirectiveEffect input,
      viz::CompositorFrameTransitionDirective::Effect* out);
};

template <>
struct StructTraits<viz::mojom::CompositorFrameTransitionDirectiveDataView,
                    viz::CompositorFrameTransitionDirective> {
  static base::TimeDelta duration(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.duration();
  }

  static uint32_t sequence_id(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.sequence_id();
  }

  static viz::CompositorFrameTransitionDirective::Type type(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.type();
  }

  static viz::CompositorFrameTransitionDirective::Effect effect(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.effect();
  }

  static bool Read(viz::mojom::CompositorFrameTransitionDirectiveDataView data,
                   viz::CompositorFrameTransitionDirective* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_
