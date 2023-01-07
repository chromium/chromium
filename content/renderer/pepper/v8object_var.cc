// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/v8object_var.h"

#include "base/check.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "ppapi/c/pp_var.h"
#include "v8/include/v8-object.h"

namespace ppapi {

// V8ObjectVar -----------------------------------------------------------------

V8ObjectVar::V8ObjectVar(PP_Instance instance,
                         v8::Local<v8::Object> v8_object)
    : instance_(content::HostGlobals::Get()->GetInstance(instance)) {
  v8_object_.Reset(instance_->GetIsolate(), v8_object);
  content::HostGlobals::Get()->host_var_tracker()->AddV8ObjectVar(this);
}

V8ObjectVar::~V8ObjectVar() {
  if (instance_)
    content::HostGlobals::Get()->host_var_tracker()->RemoveV8ObjectVar(this);
  v8_object_.Reset();
}

V8ObjectVar* V8ObjectVar::AsV8ObjectVar() {
  return this;
}

PP_VarType V8ObjectVar::GetType() const {
  return PP_VARTYPE_OBJECT;
}

v8::Local<v8::Object> V8ObjectVar::GetHandle() const {
  if (instance_)
    return v8::Local<v8::Object>::New(instance_->GetIsolate(), v8_object_);
  return v8::Local<v8::Object>();
}

void V8ObjectVar::InstanceDeleted() {
  // This is called by the HostVarTracker which will take care of removing us
  // from its set.
  DCHECK(instance_);
  instance_ = nullptr;
}

// static
scoped_refptr<V8ObjectVar> V8ObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<V8ObjectVar>(nullptr);
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return scoped_refptr<V8ObjectVar>();
  return scoped_refptr<V8ObjectVar>(var_object->AsV8ObjectVar());
}

}  // namespace ppapi
