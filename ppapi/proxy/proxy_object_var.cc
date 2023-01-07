// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_object_var.h"

#include "base/check.h"
#include "ppapi/c/pp_var.h"

using ppapi::proxy::PluginDispatcher;

namespace ppapi {

ProxyObjectVar::ProxyObjectVar(PluginDispatcher* dispatcher,
                               int32_t host_var_id)
    : dispatcher_(dispatcher), host_var_id_(host_var_id), user_data_(NULL) {
  // Should be given valid objects or we'll crash later.
  DCHECK(host_var_id_);
}

ProxyObjectVar::~ProxyObjectVar() {
}

ProxyObjectVar* ProxyObjectVar::AsProxyObjectVar() {
  return this;
}

PP_VarType ProxyObjectVar::GetType() const {
  return PP_VARTYPE_OBJECT;
}

void ProxyObjectVar::AssignVarID(int32_t id) {
  return Var::AssignVarID(id);
}

}  // namespace ppapi
