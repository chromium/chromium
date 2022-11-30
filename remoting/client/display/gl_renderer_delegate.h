// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_RENDERER_DELEGATE_H_
#define REMOTING_CLIENT_DISPLAY_GL_RENDERER_DELEGATE_H_

namespace remoting {

// Interface to interact with GlRenderer. All functions will be called on the
// display thread.
class GlRendererDelegate {
 public:
  // Called when GlRenderer is about to render a frame on current OpenGL
  // surface. Return true if GlRenderer can continue the render process.
  virtual bool CanRenderFrame() = 0;

  // Called after GlRenderer has successfully rendered a frame on current OpenGL
  // surface.
  virtual void OnFrameRendered() = 0;

  // Called when the size of the canvas (= size of desktop frame) is changed.
  virtual void OnSizeChanged(int width, int height) = 0;

 protected:
  virtual ~GlRendererDelegate() {}
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DISPLAY_GL_RENDERER_DELEGATE_H_
