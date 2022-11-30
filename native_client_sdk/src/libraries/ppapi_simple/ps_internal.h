/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_PPAPI_SIMPLE_PS_INTERNAL_H_
#define LIBRARIES_PPAPI_SIMPLE_PS_INTERNAL_H_

#include "ppapi/c/ppb.h"

EXTERN_C_BEGIN

/* Defined in ps_instance.c. */
const void* PSGetInterfaceImplementation(const char*);
extern PPB_GetInterface g_ps_get_interface;

EXTERN_C_END

#endif  // LIBRARIES_PPAPI_SIMPLE_PS_INTERNAL_H_
