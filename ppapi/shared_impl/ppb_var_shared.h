// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Var_Shared {
 public:
  static const PPB_Var_1_2* GetVarInterface1_2();
  static const PPB_Var_1_1* GetVarInterface1_1();
  static const PPB_Var_1_0* GetVarInterface1_0();
  static const PPB_VarArrayBuffer_1_0* GetVarArrayBufferInterface1_0();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_VAR_SHARED_H_
