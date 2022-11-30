// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_

#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class CONTENT_EXPORT CompositorClient {
 public:
  CompositorClient(const CompositorClient&) = delete;
  CompositorClient& operator=(const CompositorClient&) = delete;

  // Compositor is requesting client to create a new surface and call
  // SetSurface again. The existing surface if any is cleared from the
  // compositor before this call.
  virtual void RecreateSurface() {}

  // Gives the client a chance to update the layer tree host before compositing.
  virtual void UpdateLayerTreeHost() {}

  // The compositor has completed swapping a frame. This is a subset of
  // DidSwapBuffers and corresponds only to frames where Compositor submits a
  // new frame.
  virtual void DidSwapFrame(int pending_frames) {}

  // This is called on all swap buffers, regardless of cause.
  virtual void DidSwapBuffers(const gfx::Size& swap_size) {}

 protected:
  CompositorClient() {}
  virtual ~CompositorClient() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
