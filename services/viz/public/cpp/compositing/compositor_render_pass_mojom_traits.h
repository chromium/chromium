// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_RENDER_PASS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_RENDER_PASS_MOJOM_TRAITS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "services/viz/public/cpp/compositing/copy_output_request_mojom_traits.h"
#include "services/viz/public/cpp/compositing/quads_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_render_pass.mojom-shared.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CompositorRenderPassDataView,
                    std::unique_ptr<viz::CompositorRenderPass>> {
  static viz::CompositorRenderPassId id(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    DCHECK(input->id);
    return input->id;
  }

  static const gfx::Rect& output_rect(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->output_rect;
  }

  static const gfx::Rect& damage_rect(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->damage_rect;
  }

  static const gfx::Transform& transform_to_root_target(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->transform_to_root_target;
  }

  static const cc::FilterOperations& filters(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->filters;
  }

  static const cc::FilterOperations& backdrop_filters(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->backdrop_filters;
  }

  static const std::optional<gfx::RRectF>& backdrop_filter_bounds(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->backdrop_filter_bounds;
  }

  static viz::SubtreeCaptureId subtree_capture_id(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    DCHECK_LE(input->subtree_size.width(), input->output_rect.size().width());
    DCHECK_LE(input->subtree_size.height(), input->output_rect.size().height());
    return input->subtree_capture_id;
  }

  static gfx::Size subtree_size(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->subtree_size;
  }

  static std::optional<viz::ViewTransitionElementResourceId>
  view_transition_element_resource_id(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    if (!input->view_transition_element_resource_id.IsValid()) {
      return std::nullopt;
    }
    return input->view_transition_element_resource_id;
  }

  static bool has_transparent_background(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->has_transparent_background;
  }

  static bool has_per_quad_damage(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->has_per_quad_damage;
  }

  static bool cache_render_pass(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->cache_render_pass;
  }

  static bool has_damage_from_contributing_content(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->has_damage_from_contributing_content;
  }

  static bool generate_mipmap(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->generate_mipmap;
  }

  static const std::vector<std::unique_ptr<viz::CopyOutputRequest>>&
  copy_requests(const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->copy_requests;
  }

  static const viz::QuadList& quad_list(
      const std::unique_ptr<viz::CompositorRenderPass>& input) {
    return input->quad_list;
  }

  static bool Read(viz::mojom::CompositorRenderPassDataView data,
                   std::unique_ptr<viz::CompositorRenderPass>* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COMPOSITOR_RENDER_PASS_MOJOM_TRAITS_H_
