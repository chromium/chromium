/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_resource.idl modified Thu Mar 28 10:09:51 2013. */

#ifndef PPAPI_C_PP_RESOURCE_H_
#define PPAPI_C_PP_RESOURCE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the <code>PP_Resource</code> type which represents data
 * associated with the module.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * This typedef represents an opaque handle assigned by the browser to the
 * resource. The handle is guaranteed never to be 0 for a valid resource, so a
 * module can initialize it to 0 to indicate a "NULL handle." Some interfaces
 * may return a NULL resource to indicate failure.
 *
 * While a Var represents something callable to JS or from the module to
 * the DOM, a resource has no meaning or visibility outside of the module
 * interface.
 *
 * Resources are reference counted. Use <code>AddRefResource()</code>
 * and <code>ReleaseResource()</code> in <code>ppb_core.h</code> to manage the
 * reference count of a resource. The data will be automatically destroyed when
 * the internal reference count reaches 0.
 */
typedef int32_t PP_Resource;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Resource, 4);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_RESOURCE_H_ */

