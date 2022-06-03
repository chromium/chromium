// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppp_instance_combined.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {

// static
PPP_Instance_Combined* PPP_Instance_Combined::Create(
    base::RepeatingCallback<const void*(const char*)> get_interface_func) {
  // Try 1.1.
  const void* ppp_instance = get_interface_func.Run(PPP_INSTANCE_INTERFACE_1_1);
  if (ppp_instance) {
    const PPP_Instance_1_1* ppp_instance_1_1 =
        static_cast<const PPP_Instance_1_1*>(ppp_instance);
    return new PPP_Instance_Combined(*ppp_instance_1_1);
  }
  // Failing that, try 1.0.
  ppp_instance = get_interface_func.Run(PPP_INSTANCE_INTERFACE_1_0);
  if (ppp_instance) {
    const PPP_Instance_1_0* ppp_instance_1_0 =
        static_cast<const PPP_Instance_1_0*>(ppp_instance);
    return new PPP_Instance_Combined(*ppp_instance_1_0);
  }
  // No supported PPP_Instance version found.
  return NULL;
}

PPP_Instance_Combined::PPP_Instance_Combined(
    const PPP_Instance_1_0& instance_if)
    : did_change_view_1_0_(instance_if.DidChangeView) {
  instance_1_1_.DidCreate = instance_if.DidCreate;
  instance_1_1_.DidDestroy = instance_if.DidDestroy;
  instance_1_1_.DidChangeView = NULL;
  instance_1_1_.DidChangeFocus = instance_if.DidChangeFocus;
  instance_1_1_.HandleDocumentLoad = instance_if.HandleDocumentLoad;
}

PPP_Instance_Combined::PPP_Instance_Combined(
    const PPP_Instance_1_1& instance_if)
    : instance_1_1_(instance_if), did_change_view_1_0_(NULL) {}

PP_Bool PPP_Instance_Combined::DidCreate(PP_Instance instance,
                                         uint32_t argc,
                                         const char* argn[],
                                         const char* argv[]) {
  return CallWhileUnlocked(instance_1_1_.DidCreate, instance, argc, argn, argv);
}

void PPP_Instance_Combined::DidDestroy(PP_Instance instance) {
  return CallWhileUnlocked(instance_1_1_.DidDestroy, instance);
}

void PPP_Instance_Combined::DidChangeView(PP_Instance instance,
                                          PP_Resource view_changed_resource,
                                          const struct PP_Rect* position,
                                          const struct PP_Rect* clip) {
  if (instance_1_1_.DidChangeView) {
    CallWhileUnlocked(
        instance_1_1_.DidChangeView, instance, view_changed_resource);
  } else {
    CallWhileUnlocked(did_change_view_1_0_, instance, position, clip);
  }
}

void PPP_Instance_Combined::DidChangeFocus(PP_Instance instance,
                                           PP_Bool has_focus) {
  CallWhileUnlocked(instance_1_1_.DidChangeFocus, instance, has_focus);
}

PP_Bool PPP_Instance_Combined::HandleDocumentLoad(PP_Instance instance,
                                                  PP_Resource url_loader) {
  return CallWhileUnlocked(
      instance_1_1_.HandleDocumentLoad, instance, url_loader);
}

}  // namespace ppapi
