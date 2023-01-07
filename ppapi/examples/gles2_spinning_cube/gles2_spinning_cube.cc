// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/examples/gles2_spinning_cube/spinning_cube.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

// Use assert as a makeshift CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>
#include <stdint.h>

namespace {

class DemoInstance : public pp::Instance, public pp::Graphics3DClient {
 public:
  DemoInstance(PP_Instance instance);
  virtual ~DemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip);
  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    // TODO(yzshen): Handle input events.
    return true;
  }

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost();

 private:
  // GL-related functions.
  void InitGL(int32_t result);
  void Paint(int32_t result);

  pp::Size plugin_size_;
  pp::CompletionCallbackFactory<DemoInstance> callback_factory_;

  // Owned data.
  pp::Graphics3D* context_;

  SpinningCube cube_;
};

DemoInstance::DemoInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      callback_factory_(this),
      context_(NULL) {}

DemoInstance::~DemoInstance() {
  assert(glTerminatePPAPI());
  delete context_;
}

bool DemoInstance::Init(uint32_t /*argc*/,
                        const char* /*argn*/[],
                        const char* /*argv*/[]) {
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  return !!glInitializePPAPI(pp::Module::Get()->get_browser_interface());
}

void DemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& /*clip*/) {
  if (position.width() == 0 || position.height() == 0)
    return;
  plugin_size_ = position.size();

  // Initialize graphics.
  InitGL(0);
}

void DemoInstance::Graphics3DContextLost() {
  delete context_;
  context_ = NULL;
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &DemoInstance::InitGL);
  pp::Module::Get()->core()->CallOnMainThread(0, cb, 0);
}

void DemoInstance::InitGL(int32_t /*result*/) {
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

  glSetCurrentContextPPAPI(context_->pp_resource());
  cube_.Init(plugin_size_.width(), plugin_size_.height());
  Paint(PP_OK);
}

void DemoInstance::Paint(int32_t result) {
  if (result != PP_OK || !context_)
    return;

  cube_.UpdateForTimeDelta(0.02f);
  cube_.Draw();

  context_->SwapBuffers(callback_factory_.NewCallback(&DemoInstance::Paint));
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class DemoModule : public pp::Module {
 public:
  DemoModule() : Module() {}
  virtual ~DemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new DemoInstance(instance);
  }
};

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new DemoModule();
}
}  // namespace pp
