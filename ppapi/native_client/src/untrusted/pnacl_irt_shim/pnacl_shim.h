/* Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_PNACL_SHIM_H_
#define PPAPI_NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_PNACL_SHIM_H_

#include "ppapi/c/ppb.h"

/** Defines the interface exposed by the generated wrapper code. */

/* There is not a typedef for PPP_GetInterface_type, but it is currently
 * the same as PPB_GetInterface. */
typedef PPB_GetInterface PPP_GetInterface_Type;

void __set_real_Pnacl_PPBGetInterface(PPB_GetInterface real);
void __set_real_Pnacl_PPPGetInterface(PPP_GetInterface_Type real);

const void *__Pnacl_PPBGetInterface(const char *name);
const void *__Pnacl_PPPGetInterface(const char *name);

struct __PnaclWrapperInfo {
  const char* iface_macro;
  const void* wrapped_iface;
  const void* real_iface;
};

#endif  /* PPAPI_NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_PNACL_SHIM_H_ */
