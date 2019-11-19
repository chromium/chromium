// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_MOJOM_TRAITS_H_

#include <memory>

#include "base/logging.h"
#include "components/viz/common/quads/render_pass.h"
#include "services/viz/public/cpp/compositing/copy_output_request_mojom_traits.h"
#include "services/viz/public/cpp/compositing/quads_mojom_traits.h"
#include "services/viz/public/mojom/compositing/render_pass.mojom-shared.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::RenderPassDataView,
                    std::unique_ptr<viz::RenderPass>> {
  static viz::RenderPassId id(const std::unique_ptr<viz::RenderPass>& input) {
    DCHECK(input->id);
    return input->id;
  }

  static const gfx::Rect& output_rect(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->output_rect;
  }

  static const gfx::Rect& damage_rect(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->damage_rect;
  }

  static const gfx::Transform& transform_to_root_target(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->transform_to_root_target;
  }

  static const cc::FilterOperations& filters(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->filters;
  }

  static const cc::FilterOperations& backdrop_filters(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->backdrop_filters;
  }

  static base::Optional<gfx::RRectF> backdrop_filter_bounds(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->backdrop_filter_bounds;
  }

  static const gfx::ColorSpace& color_space(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->color_space;
  }

  static bool has_transparent_background(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->has_transparent_background;
  }

  static bool cache_render_pass(const std::unique_ptr<viz::RenderPass>& input) {
    return input->cache_render_pass;
  }

  static bool has_damage_from_contributing_content(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->has_damage_from_contributing_content;
  }

  static bool generate_mipmap(const std::unique_ptr<viz::RenderPass>& input) {
    return input->generate_mipmap;
  }

  static const std::vector<std::unique_ptr<viz::CopyOutputRequest>>&
  copy_requests(const std::unique_ptr<viz::RenderPass>& input) {
    return input->copy_requests;
  }

  static const viz::QuadList& quad_list(
      const std::unique_ptr<viz::RenderPass>& input) {
    return input->quad_list;
  }

  static bool Read(viz::mojom::RenderPassDataView data,
                   std::unique_ptr<viz::RenderPass>* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RENDER_PASS_MOJOM_TRAITS_H_
