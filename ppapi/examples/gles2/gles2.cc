// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <iostream>
#include <sstream>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"
#include "ppapi/utility/completion_callback_factory.h"

// Use assert as a makeshift CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>
#include <stdint.h>

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define assertNoGLError() \
  assert(!gles2_if_->GetError(context_->pp_resource()));

namespace {

class GLES2DemoInstance : public pp::Instance,
                          public pp::Graphics3DClient {
 public:
  GLES2DemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~GLES2DemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    // TODO(jamesr): What's the state of context_? Should we delete the old one
    // or try to revive it somehow?
    // For now, just delete it and construct+bind a new context.
    delete context_;
    context_ = NULL;
    pp::CompletionCallback cb = callback_factory_.NewCallback(
        &GLES2DemoInstance::InitGL);
    module_->core()->CallOnMainThread(0, cb, 0);
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP) {
      fullscreen_ = !fullscreen_;
      pp::Fullscreen(this).SetFullscreen(fullscreen_);
    }
    return true;
  }

 private:

  // GL-related functions.
  void InitGL(int32_t result);
  void FlickerAndPaint(int32_t result, bool paint_blue);

  pp::Size plugin_size_;
  pp::CompletionCallbackFactory<GLES2DemoInstance> callback_factory_;

  // Unowned pointers.
  const PPB_OpenGLES2* gles2_if_;
  pp::Module* module_;

  // Owned data.
  pp::Graphics3D* context_;
  bool fullscreen_;
};

GLES2DemoInstance::GLES2DemoInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance), pp::Graphics3DClient(this),
      callback_factory_(this),
      gles2_if_(static_cast<const PPB_OpenGLES2*>(
          module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE))),
      module_(module),
      context_(NULL),
      fullscreen_(false) {
  assert(gles2_if_);
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
}

GLES2DemoInstance::~GLES2DemoInstance() {
  delete context_;
}

void GLES2DemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  plugin_size_ = position.size();

  // Initialize graphics.
  InitGL(0);
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class GLES2DemoModule : public pp::Module {
 public:
  GLES2DemoModule() : pp::Module() {}
  virtual ~GLES2DemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new GLES2DemoInstance(instance, this);
  }
};

void GLES2DemoInstance::InitGL(int32_t result) {
  assert(plugin_size_.width() && plugin_size_.height());

  if (context_) {
    context_->ResizeBuffers(plugin_size_.width(), plugin_size_.height());
    return;
  }
  int32_t context_attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, plugin_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, plugin_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, context_attributes);
  assert(!context_->is_null());
  assert(BindGraphics(*context_));

  // Clear color bit.
  gles2_if_->ClearColor(context_->pp_resource(), 0, 1, 0, 1);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);

  assertNoGLError();

  FlickerAndPaint(0, true);
}

void GLES2DemoInstance::FlickerAndPaint(int32_t result, bool paint_blue) {
  if (result != 0 || !context_)
    return;
  float r = paint_blue ? 0 : 1.f;
  float g = 0;
  float b = paint_blue ? 1.f : 0;
  float a = 0.75;
  gles2_if_->ClearColor(context_->pp_resource(), r, g, b, a);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);
  assertNoGLError();

  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &GLES2DemoInstance::FlickerAndPaint, !paint_blue);
  context_->SwapBuffers(cb);
  assertNoGLError();
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new GLES2DemoModule();
}
}  // namespace pp
