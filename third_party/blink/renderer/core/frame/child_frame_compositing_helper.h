// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CHILD_FRAME_COMPOSITING_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/safe_non_retaining_persistent.h"

namespace cc {
class PictureLayer;
}

namespace blink {

class ChildFrameCompositor;

class CORE_EXPORT ChildFrameCompositingHelper : public cc::ContentLayerClient {
 public:
  explicit ChildFrameCompositingHelper(
      ChildFrameCompositor* child_frame_compositor);
  ChildFrameCompositingHelper(const ChildFrameCompositingHelper&) = delete;
  ChildFrameCompositingHelper& operator=(const ChildFrameCompositingHelper&) =
      delete;
  ~ChildFrameCompositingHelper() override;

  enum class AllowPaintHolding { kYes, kNo };
  void SetSurfaceId(const viz::SurfaceId& surface_id,
                    AllowPaintHolding allow_paint_holding);
  void UpdateVisibility(bool visible);
  void ChildFrameGone(float device_scale_factor);

  const viz::SurfaceId& surface_id() const { return surface_id_; }

 private:
  // cc::ContentLayerClient implementation. Called from the cc::PictureLayer
  // created for the crashed child frame to display the sad image.
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

  void MaybeSetUpPaintHolding(const viz::SurfaceId& fallback_id,
                              AllowPaintHolding allow_paint_holding);
  void PaintHoldingTimerFired();

  // The `ChildFrameCompositor` object owns this `ChildFrameCompositingHelper`.
  // `child_frame_compositor_` should never be null. We can't use a strong
  // `Persistent` here because that would create a cycle between the GCed
  // `ChildFrameCompositor` and non-GCed `ChildFrameCompositingHelper` that
  // would leak both objects.
  const SafeNonRetainingPersistent<ChildFrameCompositor>
      child_frame_compositor_;
  viz::SurfaceId surface_id_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  scoped_refptr<cc::PictureLayer> crash_ui_layer_;
  base::OneShotTimer paint_holding_timer_;
  float device_scale_factor_ = 1.f;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CHILD_FRAME_COMPOSITING_HELPER_H_
