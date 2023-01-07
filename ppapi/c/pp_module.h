/* Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_module.idl modified Sat Jul 16 16:50:26 2011. */

#ifndef PPAPI_C_PP_MODULE_H_
#define PPAPI_C_PP_MODULE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the PP_Module type which uniquely identifies the module
 * or .nexe.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * The PP_Module value uniquely identifies the module or .nexe.
 *
 * This identifier is an opaque handle assigned by the browser to the module. It
 * is guaranteed never to be 0, so a module can initialize it to 0 to
 * indicate a "NULL handle."
 */
typedef int32_t PP_Module;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Module, 4);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_MODULE_H_ */

