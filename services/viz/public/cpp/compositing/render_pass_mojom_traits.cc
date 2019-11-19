// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/render_pass_mojom_traits.h"

#include "base/numerics/safe_conversions.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::RenderPassDataView,
                  std::unique_ptr<viz::RenderPass>>::
    Read(viz::mojom::RenderPassDataView data,
         std::unique_ptr<viz::RenderPass>* out) {
  *out = viz::RenderPass::Create();
  CHECK(data.ReadOutputRect(&(*out)->output_rect));
  CHECK(data.ReadDamageRect(&(*out)->damage_rect));
  CHECK(data.ReadTransformToRootTarget(&(*out)->transform_to_root_target));
  CHECK(data.ReadFilters(&(*out)->filters));
  CHECK(data.ReadBackdropFilters(&(*out)->backdrop_filters));
  CHECK(data.ReadBackdropFilterBounds(&(*out)->backdrop_filter_bounds));
  CHECK(data.ReadColorSpace(&(*out)->color_space));
  CHECK(data.ReadCopyRequests(&(*out)->copy_requests));
  (*out)->id = data.id();
  // RenderPass ids are never zero.
  CHECK((*out)->id);
  (*out)->has_transparent_background = data.has_transparent_background();
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
    CHECK(quad);
    CHECK(quads.Read(i, quad));

    // Read the SharedQuadState.
    viz::mojom::SharedQuadStateDataView sqs_data_view;
    quad_data_view.GetSqsDataView(&sqs_data_view);
    // If there is no seralized SharedQuadState then used the last deseriaized
    // one.
    if (!sqs_data_view.is_null()) {
      last_sqs = (*out)->CreateAndAppendSharedQuadState();
      CHECK(quad_data_view.ReadSqs(last_sqs));
    }
    quad->shared_quad_state = last_sqs;
    CHECK(quad->shared_quad_state);
  }
  return true;
}

}  // namespace mojo
