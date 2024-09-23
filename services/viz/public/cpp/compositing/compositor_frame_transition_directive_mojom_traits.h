// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_

#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "services/viz/public/mojom/compositing/compositor_frame_transition_directive.mojom-shared.h"
#include "ui/gfx/display_color_spaces.h"

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
struct StructTraits<
    viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView,
    viz::CompositorFrameTransitionDirective::SharedElement> {
  static viz::CompositorRenderPassId render_pass_id(
      const viz::CompositorFrameTransitionDirective::SharedElement& element) {
    return element.render_pass_id;
  }

  static viz::ViewTransitionElementResourceId
  view_transition_element_resource_id(
      const viz::CompositorFrameTransitionDirective::SharedElement& element) {
    return element.view_transition_element_resource_id;
  }

  static bool Read(
      viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView data,
      viz::CompositorFrameTransitionDirective::SharedElement* out);
};

template <>
struct StructTraits<viz::mojom::CompositorFrameTransitionDirectiveDataView,
                    viz::CompositorFrameTransitionDirective> {
  static uint32_t sequence_id(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.sequence_id();
  }

  static viz::CompositorFrameTransitionDirective::Type type(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.type();
  }

  static blink::ViewTransitionToken transition_token(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.transition_token();
  }

  static bool maybe_cross_frame_sink(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.maybe_cross_frame_sink();
  }

  static std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
  shared_elements(const viz::CompositorFrameTransitionDirective& directive) {
    return directive.shared_elements();
  }

  static const gfx::DisplayColorSpaces& display_color_spaces(
      const viz::CompositorFrameTransitionDirective& directive) {
    return directive.display_color_spaces();
  }

  static bool Read(viz::mojom::CompositorFrameTransitionDirectiveDataView data,
                   viz::CompositorFrameTransitionDirective* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_MOJOM_TRAITS_H_
