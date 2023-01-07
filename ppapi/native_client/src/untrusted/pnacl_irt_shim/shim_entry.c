/*
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.h"


/*
 * This is the true entry point for untrusted code.
 * See nacl_startup.h for the layout at the argument pointer.
 */
void _pnacl_wrapper_start(uint32_t *info) {
  Elf32_auxv_t *auxv = nacl_startup_auxv(info);

  Elf32_auxv_t *entry = NULL;
  for (Elf32_auxv_t *av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      entry = av;
      break;
    }
  }

  if (entry != NULL) {
    /*
     * Save the real irt interface query function.
     */
    __pnacl_real_irt_query_func = (TYPE_nacl_irt_query) entry->a_un.a_val;

    /*
     * Overwrite the auxv slot with the pnacl IRT shim query function.
     */
    entry->a_type = AT_SYSINFO;
    entry->a_un.a_val = (uintptr_t) __pnacl_wrap_irt_query_func;
  }

  /* If entry is NULL still allow startup to continue.  It may be the case
   * that the IRT was not actually used (e.g., for some commandline tests).
   * For newlib, we can tell that the IRT isn't used when libnacl_sys_private.a
   * is in the bitcode link line. However, glibc does not use
   * libnacl_sys_private, so that would not work. We could look for -lppapi
   * in the bitcode link line, but looking at the bitcode link line
   * seems brittle (what if the bitcode link was separated from translation).
   * Thus we always wrap _start, even if there is no IRT auxv entry.
   */

  /*
   * Call the user entry point function.  It should not return.
   * TODO(sehr): Find a way to ensure this is invoked via a tail call.
   */
  _start(info);
}
