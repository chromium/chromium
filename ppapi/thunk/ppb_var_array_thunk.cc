// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/ppb_var_array.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var Create() {
  ProxyAutoLock lock;

  // Var tracker will hold a reference to this object.
  ArrayVar* var = new ArrayVar();
  return var->GetPPVar();
}

PP_Var Get(PP_Var array, uint32_t index) {
  ProxyAutoLock lock;

  ArrayVar* array_var = ArrayVar::FromPPVar(array);
  if (!array_var)
    return PP_MakeUndefined();
  return array_var->Get(index);
}

PP_Bool Set(PP_Var array, uint32_t index, PP_Var value) {
  ProxyAutoLock lock;

  ArrayVar* array_var = ArrayVar::FromPPVar(array);
  if (!array_var)
    return PP_FALSE;
  return array_var->Set(index, value);
}

uint32_t GetLength(PP_Var array) {
  ProxyAutoLock lock;

  ArrayVar* array_var = ArrayVar::FromPPVar(array);
  if (!array_var)
    return 0;
  return array_var->GetLength();
}

PP_Bool SetLength(PP_Var array, uint32_t length) {
  ProxyAutoLock lock;

  ArrayVar* array_var = ArrayVar::FromPPVar(array);
  if (!array_var)
    return PP_FALSE;
  return array_var->SetLength(length);
}

const PPB_VarArray_1_0 g_ppb_vararray_1_0_thunk = {
  &Create,
  &Get,
  &Set,
  &GetLength,
  &SetLength
};

}  // namespace

const PPB_VarArray_1_0* GetPPB_VarArray_1_0_Thunk() {
  return &g_ppb_vararray_1_0_thunk;
}

}  // namespace thunk
}  // namespace ppapi
