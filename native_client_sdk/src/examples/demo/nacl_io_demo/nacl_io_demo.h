/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef EXAMPLES_DEMO_NACL_IO_DEMO_NACL_IO_DEMO_H_
#define EXAMPLES_DEMO_NACL_IO_DEMO_NACL_IO_DEMO_H_

#include <stdarg.h>
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "sdk_util/macros.h"  // for PRINTF_LIKE

struct PP_Var CStrToVar(const char* str);
char* VprintfToNewString(const char* format, va_list args) PRINTF_LIKE(1, 0);
char* PrintfToNewString(const char* format, ...) PRINTF_LIKE(1, 2);
struct PP_Var GetDictVar(struct PP_Var var, const char* key);

extern PPB_Var* g_ppb_var;
extern PPB_VarArray* g_ppb_var_array;
extern PPB_VarDictionary* g_ppb_var_dictionary;

#endif  // EXAMPLES_DEMO_NACL_IO_DEMO_NACL_IO_DEMO_H_
