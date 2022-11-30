/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "ppapi_simple/ps.h"

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb.h"

/* Defined in ps_instance.c */
extern PP_Instance g_ps_instance;
extern PPB_GetInterface g_ps_get_interface;

PP_Instance PSGetInstanceId(void) {
  return g_ps_instance;
}

const void* PSGetInterface(const char *name) {
  return g_ps_get_interface(name);
}
