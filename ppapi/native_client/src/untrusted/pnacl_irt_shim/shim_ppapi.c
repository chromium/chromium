/*
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.h"

#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/irt_shim_ppapi.h"
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.h"

#define ARRAY_SIZEOF(a) (sizeof(a) / sizeof(a[0]))

/* Returns 1 if the strings s1 and s2 are equal, comparing only the first n
   bytes of s1 and s2; 0 otherwise. */
static int streq(const char *s1, const char *s2, size_t n) {
  for(; n && *s1 && *s2 && *s1 == *s2; ++s1, ++s2, --n);
  return n == 0 || *s1 == *s2;
}


TYPE_nacl_irt_query __pnacl_real_irt_query_func = NULL;

size_t __pnacl_wrap_irt_query_func(const char *interface_ident,
                                   void *table, size_t tablesize) {
  /*
   * Note there is a benign race in initializing the wrapper.
   * We build the "hook" structure by copying from the IRT's hook and then
   * writing our wrapper for the ppapi method.  Two threads may end up
   * attempting to do this simultaneously, which should not be a problem,
   * as they are writing the same values.
   */
  if (!streq(interface_ident, NACL_IRT_PPAPIHOOK_v0_1,
             ARRAY_SIZEOF(NACL_IRT_PPAPIHOOK_v0_1))) {
    /*
     * The interface is not wrapped, so use the real interface.
     */
    return (*__pnacl_real_irt_query_func)(interface_ident, table, tablesize);
  }
#ifndef PNACL_SHIM_AOT
  /*
   * For PNaCl in-the-browser, redirect to using
   * NACL_IRT_PPAPIHOOK_PNACL_PRIVATE_v0_1 instead of NACL_IRT_PPAPIHOOK_v0_1.
   */
  return (*__pnacl_real_irt_query_func)(NACL_IRT_PPAPIHOOK_PNACL_PRIVATE_v0_1,
                                        table, tablesize);
#else
  /*
   * For offline generated nexes, avoid depending on the private
   * NACL_IRT_PPAPIHOOK_PNACL_PRIVATE_v0_1 interface, and just do the
   * overriding here manually.
   */
  struct nacl_irt_ppapihook real_irt_ppapi_hook;
  if ((*__pnacl_real_irt_query_func)(NACL_IRT_PPAPIHOOK_v0_1,
                                     &real_irt_ppapi_hook,
                                     sizeof real_irt_ppapi_hook) !=
      sizeof real_irt_ppapi_hook) {
    return 0;
  }
  real_irt_ppapi_start = real_irt_ppapi_hook.ppapi_start;
  /*
   * Copy the interface structure into the client.
   */
  struct nacl_irt_ppapihook *dest = table;
  if (sizeof *dest <= tablesize) {
    dest->ppapi_start = irt_shim_ppapi_start;
    dest->ppapi_register_thread_creator =
        real_irt_ppapi_hook.ppapi_register_thread_creator;
    return sizeof *dest;
  }
  return 0;
#endif
}
