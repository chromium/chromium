// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FRAME_BLAME_CONTEXT_H_
#define CONTENT_RENDERER_FRAME_BLAME_CONTEXT_H_

#include "base/trace_event/blame_context.h"

namespace content {

class RenderFrameImpl;

// A blame context which represents a single render frame.
class FrameBlameContext : public base::trace_event::BlameContext {
 public:
  FrameBlameContext(RenderFrameImpl* frame, RenderFrameImpl* parent_frame);

  FrameBlameContext(const FrameBlameContext&) = delete;
  FrameBlameContext& operator=(const FrameBlameContext&) = delete;

  ~FrameBlameContext() override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_FRAME_BLAME_CONTEXT_H_
