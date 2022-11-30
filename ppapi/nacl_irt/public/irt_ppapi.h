/*
 * Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_NACL_IRT_PUBLIC_IRT_PPAPI_H_
#define PPAPI_NACL_IRT_PUBLIC_IRT_PPAPI_H_

#include <stddef.h>
#include <stdint.h>

#include "ppapi/c/ppp.h"

struct PP_StartFunctions {
  int32_t (*PPP_InitializeModule)(PP_Module module_id,
                                  PPB_GetInterface get_browser_interface);
  void (*PPP_ShutdownModule)(void);
  const void* (*PPP_GetInterface)(const char* interface_name);
};

struct PP_ThreadFunctions {
  /*
   * This is a cut-down version of pthread_create()/pthread_join().
   * We omit thread creation attributes and the thread's return value.
   *
   * We use uintptr_t as the thread ID type because pthread_t is not
   * part of the stable ABI; a user thread library might choose an
   * arbitrary size for its own pthread_t.
   */
  int (*thread_create)(uintptr_t* tid,
                       void (*func)(void* thread_argument),
                       void* thread_argument);
  int (*thread_join)(uintptr_t tid);
};

#define NACL_IRT_PPAPIHOOK_v0_1 "nacl-irt-ppapihook-0.1"
struct nacl_irt_ppapihook {
  int (*ppapi_start)(const struct PP_StartFunctions*);
  void (*ppapi_register_thread_creator)(const struct PP_ThreadFunctions*);
};

#endif  // PPAPI_NACL_IRT_PUBLIC_IRT_PPAPI_H_
