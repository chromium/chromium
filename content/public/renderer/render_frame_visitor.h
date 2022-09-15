// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_VISITOR_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_VISITOR_H_

namespace content {

class RenderFrame;

class RenderFrameVisitor {
 public:
  // Return true to continue visiting RenderFrames or false to stop.
  virtual bool Visit(RenderFrame* render_frame) = 0;

 protected:
  virtual ~RenderFrameVisitor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_VISITOR_H_
