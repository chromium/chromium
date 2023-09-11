// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_mojom_traits.h"

#include "services/viz/public/cpp/crash_keys.h"

namespace mojo {

bool RenderPassExists(viz::CompositorRenderPassId pass_id,
                      const viz::CompositorRenderPassList& render_passes) {
  for (const auto& pass : render_passes) {
    if (pass->id == pass_id)
      return true;
  }

  return false;
}

// static
bool StructTraits<viz::mojom::CompositorFrameDataView, viz::CompositorFrame>::
    Read(viz::mojom::CompositorFrameDataView data, viz::CompositorFrame* out) {
  if (!data.ReadPasses(&out->render_pass_list))
    return false;

  if (out->render_pass_list.empty()) {
    viz::SetDeserializationCrashKeyString(
        "CompositorFrame::render_pass_list empty");
    return false;
  }

  if (out->render_pass_list.back()->output_rect.size().IsEmpty()) {
    viz::SetDeserializationCrashKeyString("CompositorFrame empty");
    return false;
  }

  if (!data.ReadMetadata(&out->metadata))
    return false;

  // Ensure that all render passes referenced by shared elements are present in
  // the CompositorFrame.
  for (const auto& directive : out->metadata.transition_directives) {
    if (directive.type() !=
        viz::CompositorFrameTransitionDirective::Type::kSave) {
      DCHECK(directive.shared_elements().empty());
      continue;
    }

    for (const auto& shared_element : directive.shared_elements()) {
      if (shared_element.render_pass_id.is_null())
        continue;

      if (!RenderPassExists(shared_element.render_pass_id,
                            out->render_pass_list)) {
        return false;
      }
    }
  }

  // Ensure that all region capture bounds are contained within the compositor
  // frame.
  gfx::Rect compositor_frame_bounds(out->size_in_pixels());
  for (const auto& crop_id_and_bounds : out->metadata.capture_bounds.bounds()) {
    if (!compositor_frame_bounds.Contains(crop_id_and_bounds.second)) {
      return false;
    }
  }

  if (!data.ReadResources(&out->resource_list)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrame::resource_list");
    return false;
  }

  return true;
}

}  // namespace mojo
