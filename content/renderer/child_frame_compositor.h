// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITOR_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITOR_H_

namespace cc {
class Layer;
}

namespace content {

// A ChildFrameCompositor is an owner of a cc::Layer that embeds a child
// frame.
class ChildFrameCompositor {
 public:
  // Get the child frame's cc::Layer.
  virtual cc::Layer* GetLayer() = 0;

  // Passes ownership of a cc::Layer to the ChildFrameCompositor.
  virtual void SetLayer(scoped_refptr<cc::Layer> layer,
                        bool is_surface_layer) = 0;

  // Returns a sad page bitmap used when the child frame has crashed.
  virtual SkBitmap* GetSadPageBitmap() = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITOR_H_
