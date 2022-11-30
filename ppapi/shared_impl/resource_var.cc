// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_var.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

int ResourceVar::GetPendingRendererHostId() const { return 0; }

int ResourceVar::GetPendingBrowserHostId() const { return 0; }

const IPC::Message* ResourceVar::GetCreationMessage() const { return NULL; }

ResourceVar* ResourceVar::AsResourceVar() { return this; }

PP_VarType ResourceVar::GetType() const { return PP_VARTYPE_RESOURCE; }

// static
ResourceVar* ResourceVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_RESOURCE)
    return NULL;
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsResourceVar();
}

ResourceVar::ResourceVar() {}

ResourceVar::~ResourceVar() {}

}  // namespace ppapi
