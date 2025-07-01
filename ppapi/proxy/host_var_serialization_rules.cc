// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_var_serialization_rules.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

using ppapi::PpapiGlobals;
using ppapi::VarTracker;

namespace ppapi {
namespace proxy {

HostVarSerializationRules::HostVarSerializationRules() {
}

HostVarSerializationRules::~HostVarSerializationRules() {
}

PP_Var HostVarSerializationRules::SendCallerOwned(const PP_Var& var) {
  return var;
}

PP_Var HostVarSerializationRules::BeginReceiveCallerOwned(const PP_Var& var) {
  return var;
}

void HostVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type != PP_VARTYPE_OBJECT && var.type >= PP_VARTYPE_STRING) {
    // Release our reference to the local Var.
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
  }
}

PP_Var HostVarSerializationRules::ReceivePassRef(const PP_Var& var) {
  // See PluginVarSerialization::BeginSendPassRef for an example.
  if (var.type == PP_VARTYPE_OBJECT)
    PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
  return var;
}

PP_Var HostVarSerializationRules::BeginSendPassRef(const PP_Var& var) {
  return var;
}

void HostVarSerializationRules::EndSendPassRef(const PP_Var& var) {
  // See PluginVarSerializationRules::ReceivePassRef for an example. We don't
  // need to do anything here for "Object" vars; we continue holding one ref on
  // behalf of the plugin.
  if (var.type != PP_VARTYPE_OBJECT) {
    // But for other ref-counted types (like String, Array, and Dictionary),
    // the value will be re-constituted on the other side as a new Var with no
    // connection to the host-side reference counting. We must therefore release
    // our reference count; this is roughly equivalent to passing the ref to the
    // plugin.
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
  }
}

void HostVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
}

}  // namespace proxy
}  // namespace ppapi
