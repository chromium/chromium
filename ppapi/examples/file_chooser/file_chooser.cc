// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/utility/completion_callback_factory.h"

class MyInstance : public pp::InstancePrivate {
 public:
  MyInstance(PP_Instance instance)
      : pp::InstancePrivate(instance) {
    callback_factory_.Initialize(this);
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
        pp::MouseInputEvent mouse_event(event);
        if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT)
          ShowFileChooser(false);
        else if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_RIGHT)
          ShowFileChooser(true);
        else
          return false;

        return true;
      }
      default:
        return false;
    }
  }

 private:
  void ShowFileChooser(bool multi_select) {
    RecreateConsole();

    PP_FileChooserMode_Dev mode =
        (multi_select ? PP_FILECHOOSERMODE_OPENMULTIPLE
                      : PP_FILECHOOSERMODE_OPEN);
    std::string accept_types = (multi_select ? "" : "text/plain");

    chooser_ = pp::FileChooser_Dev(this, mode, accept_types);
    chooser_.Show(callback_factory_.NewCallbackWithOutput(
        &MyInstance::ShowSelectedFileNames));
  }

  void ShowSelectedFileNames(int32_t result,
                             const std::vector<pp::FileRef>& files) {
    if (result != PP_OK)
      return;
    for (size_t i = 0; i < files.size(); i++)
      Log(files[i].GetName());
  }

  void RecreateConsole() {
    pp::VarPrivate doc = GetWindowObject().GetProperty("document");
    pp::VarPrivate body = doc.GetProperty("body");
    if (!console_.is_undefined())
      body.Call("removeChild", console_);

    console_ = doc.Call("createElement", "pre");
    console_.SetProperty("id", "console");
    console_.GetProperty("style").SetProperty("backgroundColor", "lightgray");
    body.Call("appendChild", console_);
  }

  void Log(const pp::Var& var) {
    pp::VarPrivate doc = GetWindowObject().GetProperty("document");
    console_.Call("appendChild", doc.Call("createTextNode", var));
    console_.Call("appendChild", doc.Call("createTextNode", "\n"));
  }

  pp::FileChooser_Dev chooser_;

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;
  pp::VarPrivate console_;
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
