/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "ppapi/native_client/src/untrusted/irt_stub/thread_creator.h"

static void fatal_error(const char *message) {
  ssize_t wrote __attribute__((unused));
  wrote = write(2, message, strlen(message));
  _exit(127);
}

/*
 * TODO(mcgrathr): This extremely stupid function should not exist.
 * If the startup calling sequence were sane, this would be done
 * someplace that has the initial pointer locally rather than stealing
 * it from environ.
 * See http://code.google.com/p/nativeclient/issues/detail?id=651
 */
static Elf32_auxv_t *find_auxv(void) {
  /*
   * This presumes environ has its startup-time value on the stack.
   */
  char **ep = environ;
  while (*ep != NULL)
    ++ep;
  return (void *) (ep + 1);
}

/*
 * Scan the auxv for AT_SYSINFO, which is the pointer to the IRT query function.
 * TODO(mcgrathr): Could get this from __nacl_irt_query, where the libnacl
 * startup code stored it, but that would have to be also added as part of
 * the glibc ABI.
 */
static TYPE_nacl_irt_query grok_auxv(const Elf32_auxv_t *auxv) {
  const Elf32_auxv_t *av;
  for (av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO)
      return (TYPE_nacl_irt_query) av->a_un.a_val;
  }
  return NULL;
}

int PpapiPluginStart(const struct PP_StartFunctions *funcs) {
  TYPE_nacl_irt_query query_func = grok_auxv(find_auxv());

  if (NULL == query_func)
    fatal_error("PpapiPluginStart: No AT_SYSINFO item found in auxv, "
                "so cannot start PPAPI.  Is the IRT library not present?\n");

  struct nacl_irt_ppapihook hooks;
  if (sizeof(hooks) != query_func(NACL_IRT_PPAPIHOOK_v0_1,
                                  &hooks, sizeof(hooks)))
    fatal_error("PpapiPluginStart: PPAPI hooks not found\n");

  __nacl_register_thread_creator(&hooks);

  return hooks.ppapi_start(funcs);
}
