// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/frame_timing_details_mojom_traits.h"

#include "ui/gfx/mojom/presentation_feedback_mojom_traits.h"
#include "ui/gfx/mojom/swap_timings_mojom_traits.h"

namespace mojo {

using Traits = StructTraits<viz::mojom::FrameTimingDetailsDataView,
                            viz::FrameTimingDetails>;
// static
bool Traits::Read(viz::mojom::FrameTimingDetailsDataView data,
                  viz::FrameTimingDetails* out) {
  return data.ReadReceivedCompositorFrameTimestamp(
             &out->received_compositor_frame_timestamp) &&
         data.ReadDrawStartTimestamp(&out->draw_start_timestamp) &&
         data.ReadSwapTimings(&out->swap_timings) &&
         data.ReadPresentationFeedback(&out->presentation_feedback);
}

}  // namespace mojo
