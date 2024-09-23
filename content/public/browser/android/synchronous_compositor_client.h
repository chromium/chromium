// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {
class TouchHandleDrawable;
}

namespace viz {
class FrameSinkId;
class CopyOutputRequest;
}

namespace content {

class SynchronousCompositor;

class SynchronousCompositorClient {
 public:
  SynchronousCompositorClient(const SynchronousCompositorClient&) = delete;
  SynchronousCompositorClient& operator=(const SynchronousCompositorClient&) =
      delete;

  // Indication to the client that |compositor| is now initialized on the
  // compositor thread, and open for business. |process_id| and |routing_id|
  // belong to the RVH that owns the compositor.
  virtual void DidInitializeCompositor(SynchronousCompositor* compositor,
                                       const viz::FrameSinkId& id) = 0;

  // Indication to the client that |compositor| is going out of scope, and
  // must not be accessed within or after this call.
  // NOTE if the client goes away before the compositor it must call
  // SynchronousCompositor::SetClient(nullptr) to release the back pointer.
  virtual void DidDestroyCompositor(SynchronousCompositor* compositor,
                                    const viz::FrameSinkId& id) = 0;

  virtual void UpdateRootLayerState(SynchronousCompositor* compositor,
                                    const gfx::PointF& total_scroll_offset,
                                    const gfx::PointF& max_scroll_offset,
                                    const gfx::SizeF& scrollable_size,
                                    float page_scale_factor,
                                    float min_page_scale_factor,
                                    float max_page_scale_factor) = 0;

  virtual void DidOverscroll(SynchronousCompositor* compositor,
                             const gfx::Vector2dF& accumulated_overscroll,
                             const gfx::Vector2dF& latest_overscroll_delta,
                             const gfx::Vector2dF& current_fling_velocity) = 0;

  virtual void PostInvalidate(SynchronousCompositor* compositor) = 0;

  virtual void DidUpdateContent(SynchronousCompositor* compositor) = 0;

  virtual ui::TouchHandleDrawable* CreateDrawable() = 0;

  virtual void CopyOutput(
      SynchronousCompositor* compositor,
      std::unique_ptr<viz::CopyOutputRequest> copy_request) = 0;

  virtual void AddBeginFrameCompletionCallback(base::OnceClosure callback) = 0;

  virtual void SetThreadIds(const std::vector<int32_t>& thread_ids) = 0;

 protected:
  SynchronousCompositorClient() {}
  virtual ~SynchronousCompositorClient() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_CLIENT_H_
