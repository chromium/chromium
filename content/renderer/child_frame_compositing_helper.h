// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"

namespace gfx {
class Size;
}

namespace viz {
class SurfaceId;
}

namespace content {

class ChildFrameCompositor;

class CONTENT_EXPORT ChildFrameCompositingHelper {
 public:
  explicit ChildFrameCompositingHelper(
      ChildFrameCompositor* child_frame_compositor);

  virtual ~ChildFrameCompositingHelper();

  void SetSurfaceId(const viz::SurfaceId& surface_id,
                    const gfx::Size& frame_size_in_dip,
                    const cc::DeadlinePolicy& deadline);
  void SetOldestAcceptableFallback(const viz::SurfaceId& surface_id,
                                   const gfx::Size& frame_size_in_dip);
  void UpdateVisibility(bool visible);
  void ChildFrameGone(const gfx::Size& frame_size_in_dip,
                      float device_scale_factor);

  const viz::SurfaceId& surface_id() const { return primary_surface_id_; }

  const viz::SurfaceId& oldest_acceptable_fallback() const {
    return fallback_surface_id_;
  }

 private:
  ChildFrameCompositor* const child_frame_compositor_;
  viz::SurfaceId primary_surface_id_;
  viz::SurfaceId fallback_surface_id_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
