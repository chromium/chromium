// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that the single accessor, Module::Get(), is not actually implemented
// in this file.  This is an intentional hook that allows users of ppapi's
// C++ wrapper objects to provide different semantics for how the singleton
// object is accessed.
//
// In general, users of ppapi will also link in ppp_entrypoints.cc, which
// provides a simple default implementation of Module::Get().
//
// A notable exception where the default ppp_entrypoints will not work is
// when implementing "internal plugins" that are statically linked into the
// browser. In this case, the process may actually have multiple Modules
// loaded at once making a traditional "singleton" unworkable.  To get around
// this, the users of ppapi need to get creative about how to properly
// implement the Module::Get() so that ppapi's C++ wrappers can find the
// right Module object.  One example solution is to use thread local storage
// to change the Module* returned based on which thread is invoking the
// function. Leaving Module::Get() unimplemented provides a hook for
// implementing such behavior.

#include "ppapi/cpp/module.h"

#include <string.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"

namespace pp {

// PPP_InputEvent implementation -----------------------------------------------

PP_Bool InputEvent_HandleEvent(PP_Instance pp_instance, PP_Resource resource) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return PP_FALSE;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return PP_FALSE;

  return PP_FromBool(instance->HandleInputEvent(InputEvent(resource)));
}

const PPP_InputEvent input_event_interface = {
  &InputEvent_HandleEvent
};

// PPP_Instance implementation -------------------------------------------------

PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                           uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return PP_FALSE;

  Instance* instance = module_singleton->CreateInstance(pp_instance);
  if (!instance)
    return PP_FALSE;
  module_singleton->current_instances_[pp_instance] = instance;
  return PP_FromBool(instance->Init(argc, argn, argv));
}

void Instance_DidDestroy(PP_Instance instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Module::InstanceMap::iterator found =
      module_singleton->current_instances_.find(instance);
  if (found == module_singleton->current_instances_.end())
    return;

  // Remove it from the map before deleting to try to catch reentrancy.
  Instance* obj = found->second;
  module_singleton->current_instances_.erase(found);
  delete obj;
}

void Instance_DidChangeView(PP_Instance pp_instance,
                            PP_Resource view_resource) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return;
  instance->DidChangeView(View(view_resource));
}

void Instance_DidChangeFocus(PP_Instance pp_instance, PP_Bool has_focus) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return;
  instance->DidChangeFocus(PP_ToBool(has_focus));
}

PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                    PP_Resource pp_url_loader) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return PP_FALSE;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return PP_FALSE;
  return PP_FromBool(instance->HandleDocumentLoad(URLLoader(pp_url_loader)));
}

static PPP_Instance instance_interface = {
  &Instance_DidCreate,
  &Instance_DidDestroy,
  &Instance_DidChangeView,
  &Instance_DidChangeFocus,
  &Instance_HandleDocumentLoad
};

// PPP_Messaging implementation ------------------------------------------------

void Messaging_HandleMessage(PP_Instance pp_instance, PP_Var var) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return;
  instance->HandleMessage(Var(PASS_REF, var));
}

static PPP_Messaging instance_messaging_interface = {
  &Messaging_HandleMessage
};

// Module ----------------------------------------------------------------------

Module::Module() : pp_module_(0), get_browser_interface_(NULL), core_(NULL) {
}

Module::~Module() {
  delete core_;
  core_ = NULL;
}

bool Module::Init() {
  return true;
}

const void* Module::GetPluginInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INPUT_EVENT_INTERFACE) == 0)
    return &input_event_interface;
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &instance_interface;
  if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0)
    return &instance_messaging_interface;

  // Now see if anything was dynamically registered.
  InterfaceMap::const_iterator found = additional_interfaces_.find(
      std::string(interface_name));
  if (found != additional_interfaces_.end())
    return found->second;

  return NULL;
}

const void* Module::GetBrowserInterface(const char* interface_name) {
  return get_browser_interface_(interface_name);
}

Instance* Module::InstanceForPPInstance(PP_Instance instance) {
  InstanceMap::iterator found = current_instances_.find(instance);
  if (found == current_instances_.end())
    return NULL;
  return found->second;
}

void Module::AddPluginInterface(const std::string& interface_name,
                                const void* vtable) {
  // Verify that we're not trying to register an interface that's already
  // handled, and if it is, that we're re-registering with the same vtable.
  // Calling GetPluginInterface rather than looking it up in the map allows
  // us to also catch "internal" ones in addition to just previously added ones.
  const void* existing_interface = GetPluginInterface(interface_name.c_str());
  if (existing_interface) {
    PP_DCHECK(vtable == existing_interface);
    return;
  }
  additional_interfaces_[interface_name] = vtable;
}

bool Module::InternalInit(PP_Module mod,
                          PPB_GetInterface get_browser_interface) {
  pp_module_ = mod;
  get_browser_interface_ = get_browser_interface;

  // Get the core interface which we require to run.
  const PPB_Core* core = reinterpret_cast<const PPB_Core*>(
      GetBrowserInterface(PPB_CORE_INTERFACE_1_0));
  if (!core)
    return false;
  core_ = new Core(core);

  return Init();
}

}  // namespace pp
