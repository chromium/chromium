// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/delegated_ink_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::DelegatedInkMetadataDataView,
                  std::unique_ptr<viz::DelegatedInkMetadata>>::
    Read(viz::mojom::DelegatedInkMetadataDataView data,
         std::unique_ptr<viz::DelegatedInkMetadata>* out) {
  // Diameter isn't expected to ever be below 0, so stop here if it is in order
  // to avoid unexpected calculations in viz.
  if (data.diameter() < 0)
    return false;

  gfx::PointF point;
  base::TimeTicks timestamp;
  gfx::RectF presentation_area;
  SkColor color;
  base::TimeTicks frame_time;
  if (!data.ReadPoint(&point) || !data.ReadTimestamp(&timestamp) ||
      !data.ReadPresentationArea(&presentation_area) ||
      !data.ReadColor(&color) || !data.ReadFrameTime(&frame_time)) {
    return false;
  }
  *out = std::make_unique<viz::DelegatedInkMetadata>(
      point, data.diameter(), color, timestamp, presentation_area, frame_time,
      data.is_hovering());
  return true;
}

}  // namespace mojo
