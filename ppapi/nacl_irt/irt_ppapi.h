// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_IRT_PPAPI_H_
#define PPAPI_NACL_IRT_IRT_PPAPI_H_

#include "ppapi/nacl_irt/public/irt_ppapi.h"

#ifdef __cplusplus
extern "C" {
#endif

int irt_ppapi_start(const struct PP_StartFunctions* funcs);

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs);

#ifdef __cplusplus
}
#endif

#endif  // PPAPI_NACL_IRT_IRT_PPAPI_H_
