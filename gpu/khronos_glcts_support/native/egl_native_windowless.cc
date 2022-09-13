// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Using egl_native from gles2_conform_support
// TODO: We may want to phase out the old gles2_conform support in preference
// of this implementation.  So eventually we'll need to move the egl_native
// stuff here or to a shareable location/path.
#include "gpu/gles2_conform_support/egl/test_support.h"

#include "third_party/khronos_glcts/framework/egl/tcuEglPlatform.hpp"

namespace egl {
namespace native {
namespace windowless {

class Surface : public tcu::egl::WindowSurface {
 public:
  Surface(tcu::egl::Display& display,
          EGLConfig config,
          const EGLint* attribList,
          int width,
          int height)
      : tcu::egl::WindowSurface(display,
                                config,
                                (EGLNativeWindowType) nullptr,
                                attribList),
        width_(width),
        height_(height) {}

  int getWidth() const override { return width_; }

  int getHeight() const override { return height_; }

 private:
  const int width_;
  const int height_;
};

class Window : public tcu::NativeWindow {
 public:
  Window(tcu::egl::Display& display,
         EGLConfig config,
         const EGLint* attribList,
         int width,
         int height)
      : tcu::NativeWindow::NativeWindow(),
        eglDisplay_(display),
        surface_(display, config, attribList, width, height) {}

  ~Window() override {}

  tcu::egl::Display& getEglDisplay() override { return eglDisplay_; }

  tcu::egl::WindowSurface& getEglSurface() override { return surface_; }

  void processEvents() override { return; }

 private:
  tcu::egl::Display& eglDisplay_;
  Surface surface_;
};

class Platform : public tcu::EglPlatform {
 public:
  Platform() : tcu::EglPlatform::EglPlatform() {}

  ~Platform() override {}

  tcu::NativeWindow* createWindow(tcu::NativeDisplay& dpy,
                                  EGLConfig config,
                                  const EGLint* attribList,
                                  int width,
                                  int height,
                                  qpVisibility visibility) override {
    tcu::egl::Display& eglDisplay = dpy.getEglDisplay();
    EGLDisplay display = eglDisplay.getEGLDisplay();
    CommandBufferGLESSetNextCreateWindowSurfaceCreatesPBuffer(display, width,
                                                              height);
    return new Window(eglDisplay, config, attribList, width, height);
  }
};

}  // namespace windowless
}  // namespace native
}  // namespace egl

tcu::Platform* createPlatform(void) {
  return new egl::native::windowless::Platform();
}
