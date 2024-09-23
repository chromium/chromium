// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_render_pass_mojom_traits.h"

#include "base/numerics/safe_conversions.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_quad_state_mojom_traits.h"
#include "services/viz/public/cpp/compositing/subtree_capture_id_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"
#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::CompositorRenderPassDataView,
                  std::unique_ptr<viz::CompositorRenderPass>>::
    Read(viz::mojom::CompositorRenderPassDataView data,
         std::unique_ptr<viz::CompositorRenderPass>* out) {
  *out = viz::CompositorRenderPass::Create();
  std::optional<viz::ViewTransitionElementResourceId>
      opt_view_transition_element_resource_id;
  if (!data.ReadOutputRect(&(*out)->output_rect) ||
      !data.ReadDamageRect(&(*out)->damage_rect) ||
      !data.ReadTransformToRootTarget(&(*out)->transform_to_root_target) ||
      !data.ReadFilters(&(*out)->filters) ||
      !data.ReadBackdropFilters(&(*out)->backdrop_filters) ||
      !data.ReadBackdropFilterBounds(&(*out)->backdrop_filter_bounds) ||
      !data.ReadSubtreeCaptureId(&(*out)->subtree_capture_id) ||
      !data.ReadSubtreeSize(&(*out)->subtree_size) ||
      !data.ReadCopyRequests(&(*out)->copy_requests) ||
      !data.ReadViewTransitionElementResourceId(
          &opt_view_transition_element_resource_id) ||
      !data.ReadId(&(*out)->id)) {
    return false;
  }

  if (opt_view_transition_element_resource_id) {
    (*out)->view_transition_element_resource_id =
        *opt_view_transition_element_resource_id;
  }

  // CompositorRenderPass ids are never zero.
  if (!(*out)->id) {
    viz::SetDeserializationCrashKeyString("Invalid render pass ID");
    return false;
  }
  if ((*out)->subtree_size.width() > (*out)->output_rect.size().width() ||
      (*out)->subtree_size.height() > (*out)->output_rect.size().height()) {
    return false;
  }

  (*out)->has_transparent_background = data.has_transparent_background();
  (*out)->has_per_quad_damage = data.has_per_quad_damage();

  (*out)->cache_render_pass = data.cache_render_pass();
  (*out)->has_damage_from_contributing_content =
      data.has_damage_from_contributing_content();
  (*out)->generate_mipmap = data.generate_mipmap();

  mojo::ArrayDataView<viz::mojom::DrawQuadDataView> quads;
  data.GetQuadListDataView(&quads);
  viz::SharedQuadState* last_sqs = nullptr;
  for (size_t i = 0; i < quads.size(); ++i) {
    viz::mojom::DrawQuadDataView quad_data_view;
    quads.GetDataView(i, &quad_data_view);
    viz::mojom::DrawQuadStateDataView quad_state_data_view;
    quad_data_view.GetDrawQuadStateDataView(&quad_state_data_view);

    viz::DrawQuad* quad =
        AllocateAndConstruct(quad_state_data_view.tag(), &(*out)->quad_list);
    if (!quad) {
      viz::SetDeserializationCrashKeyString("AllocateAndConstruct quad failed");
      return false;
    }
    if (!quads.Read(i, quad))
      return false;

    // Read the SharedQuadState.
    viz::mojom::SharedQuadStateDataView sqs_data_view;
    quad_data_view.GetSqsDataView(&sqs_data_view);
    // If there is no serialized SharedQuadState then use the last deserialized
    // one.
    if (!sqs_data_view.is_null()) {
      using SqsTraits = StructTraits<viz::mojom::SharedQuadStateDataView,
                                     viz::SharedQuadState>;
      last_sqs = (*out)->CreateAndAppendSharedQuadState();
      if (!SqsTraits::Read(sqs_data_view, last_sqs))
        return false;
    }
    quad->shared_quad_state = last_sqs;
    if (!quad->shared_quad_state) {
      viz::SetDeserializationCrashKeyString("No shared quad state");
      return false;
    }
  }
  return true;
}

}  // namespace mojo
