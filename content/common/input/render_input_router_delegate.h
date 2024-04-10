// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
#define CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_

#include "cc/trees/render_frame_metadata.h"

namespace content {

class RenderWidgetHostViewInput;

class CONTENT_EXPORT RenderInputRouterDelegate {
 public:
  virtual ~RenderInputRouterDelegate() = default;

  virtual RenderWidgetHostViewInput* GetPointerLockView() = 0;
  // TODO(b/331419617): Use a new FrameMetadataBase class instead of
  // RenderFrameMetadata.
  virtual const cc::RenderFrameMetadata& GetLastRenderFrameMetadata() = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
