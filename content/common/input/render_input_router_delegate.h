// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
#define CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_

#include "cc/trees/render_frame_metadata.h"
#include "ui/gfx/delegated_ink_point.h"

namespace content {

class RenderWidgetHostViewInput;
class RenderInputRouterIterator;

class CONTENT_EXPORT RenderInputRouterDelegate {
 public:
  virtual ~RenderInputRouterDelegate() = default;

  virtual RenderWidgetHostViewInput* GetPointerLockView() = 0;
  // TODO(b/331419617): Use a new FrameMetadataBase class instead of
  // RenderFrameMetadata.
  virtual const cc::RenderFrameMetadata& GetLastRenderFrameMetadata() = 0;

  virtual std::unique_ptr<RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters() = 0;

  // Forwards |delegated_ink_point| to viz over IPC to be drawn as part of
  // delegated ink trail, resetting the |ended_delegated_ink_trail| flag.
  virtual void ForwardDelegatedInkPoint(
      gfx::DelegatedInkPoint& delegated_ink_point,
      bool& ended_delegated_ink_trail) = 0;
  // Instructs viz to reset prediction for delegated ink trails, indicating that
  // the trail has ended. Updates the |ended_delegated_ink_trail| flag to
  // reflect this change.
  virtual void ResetDelegatedInkPointPrediction(
      bool& ended_delegated_ink_trail) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
