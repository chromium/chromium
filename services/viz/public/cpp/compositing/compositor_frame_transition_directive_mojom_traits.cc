// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/view_transition_element_resource_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_frame_transition_directive.mojom-shared.h"

namespace mojo {

// static
viz::mojom::CompositorFrameTransitionDirectiveType
EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveType,
           viz::CompositorFrameTransitionDirective::Type>::
    ToMojom(viz::CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case viz::CompositorFrameTransitionDirective::Type::kSave:
      return viz::mojom::CompositorFrameTransitionDirectiveType::kSave;
    case viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return viz::mojom::CompositorFrameTransitionDirectiveType::
          kAnimateRenderer;
    case viz::CompositorFrameTransitionDirective::Type::kRelease:
      return viz::mojom::CompositorFrameTransitionDirectiveType::kRelease;
  }
  NOTREACHED();
  return viz::mojom::CompositorFrameTransitionDirectiveType::kSave;
}

// static
bool EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveType,
                viz::CompositorFrameTransitionDirective::Type>::
    FromMojom(viz::mojom::CompositorFrameTransitionDirectiveType input,
              viz::CompositorFrameTransitionDirective::Type* out) {
  switch (input) {
    case viz::mojom::CompositorFrameTransitionDirectiveType::kSave:
      *out = viz::CompositorFrameTransitionDirective::Type::kSave;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer:
      *out = viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveType::kRelease:
      *out = viz::CompositorFrameTransitionDirective::Type::kRelease;
      return true;
  }
  return false;
}

// static
bool StructTraits<
    viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView,
    viz::CompositorFrameTransitionDirective::SharedElement>::
    Read(viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView
             data,
         viz::CompositorFrameTransitionDirective::SharedElement* out) {
  return data.ReadRenderPassId(&out->render_pass_id) &&
         data.ReadViewTransitionElementResourceId(
             &out->view_transition_element_resource_id);
}

// static
bool StructTraits<viz::mojom::CompositorFrameTransitionDirectiveDataView,
                  viz::CompositorFrameTransitionDirective>::
    Read(viz::mojom::CompositorFrameTransitionDirectiveDataView data,
         viz::CompositorFrameTransitionDirective* out) {
  uint32_t sequence_id = data.sequence_id();

  absl::optional<viz::NavigationID> navigation_id;
  viz::CompositorFrameTransitionDirective::Type type;
  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements;
  if (!data.ReadNavigationId(&navigation_id) || !data.ReadType(&type) ||
      !data.ReadSharedElements(&shared_elements)) {
    return false;
  }

  *out = viz::CompositorFrameTransitionDirective(
      navigation_id ? *navigation_id : viz::NavigationID::Null(), sequence_id,
      type, std::move(shared_elements));
  return true;
}

}  // namespace mojo
