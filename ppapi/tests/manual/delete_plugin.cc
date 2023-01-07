// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/trusted/ppb_instance_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/var_private.h"

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance) : pp::Instance(instance) {
    factory_.Initialize(this);
  }

  virtual ~MyInstance() {
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  virtual bool HandleInputEvent(const PP_InputEvent& event) {
    switch (event.type) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        pp::Module::Get()->core()->CallOnMainThread(100,
            factory_.NewCallback(&MyInstance::SayHello));
        return true;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        return true;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        return true;
      default:
        return false;
    }
  }

 private:
  void SayHello(int32_t) {
    pp::Var code("deletePlugin()");
    /*
    When scripting is removed from instance, this is the code that will do the
    same thing:
    const PPB_Instance_Trusted* inst =
        (const PPB_Instance_Trusted*)pp::Module::Get()->GetBrowserInterface(
            PPB_INSTANCE_TRUSTED_INTERFACE);
    inst->ExecuteScript(pp_instance(), code.pp_var(), NULL);
    */
    ExecuteScript(code);
  }

  pp::CompletionCallbackFactory<MyInstance> factory_;
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
