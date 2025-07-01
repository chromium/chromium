// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/instance_private.h"

#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/private/var_private.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Instance_Private>() {
  return PPB_INSTANCE_PRIVATE_INTERFACE;
}

PP_Var GetInstanceObject(PP_Instance pp_instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return Var().Detach();
  InstancePrivate* instance_private = static_cast<InstancePrivate*>(
      module_singleton->InstanceForPPInstance(pp_instance));
  if (!instance_private)
    return Var().Detach();
  return instance_private->GetInstanceObject().Detach();
}

const PPP_Instance_Private ppp_instance_private = {
  &GetInstanceObject
};

}  // namespace

InstancePrivate::InstancePrivate(PP_Instance instance) : Instance(instance) {
  // If at least 1 InstancePrivate is created, register the PPP_INSTANCE_PRIVATE
  // interface.
  Module::Get()->AddPluginInterface(PPP_INSTANCE_PRIVATE_INTERFACE,
                                    &ppp_instance_private);
}

InstancePrivate::~InstancePrivate() {}

Var InstancePrivate::GetInstanceObject() {
  return Var();
}

VarPrivate InstancePrivate::GetWindowObject() {
  if (!has_interface<PPB_Instance_Private>())
    return VarPrivate();
  return VarPrivate(PASS_REF,
      get_interface<PPB_Instance_Private>()->GetWindowObject(pp_instance()));
}

VarPrivate InstancePrivate::GetOwnerElementObject() {
  if (!has_interface<PPB_Instance_Private>())
    return VarPrivate();
  return VarPrivate(PASS_REF,
      get_interface<PPB_Instance_Private>()->GetOwnerElementObject(
          pp_instance()));
}

VarPrivate InstancePrivate::ExecuteScript(const Var& script, Var* exception) {
  if (!has_interface<PPB_Instance_Private>())
    return VarPrivate();
  return VarPrivate(PASS_REF,
      get_interface<PPB_Instance_Private>()->ExecuteScript(
          pp_instance(),
          script.pp_var(),
          VarPrivate::OutException(exception).get()));
}

}  // namespace pp
