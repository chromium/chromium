/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_var_array.idl modified Sun Jun 16 15:37:27 2013. */

#ifndef PPAPI_C_PPB_VAR_ARRAY_H_
#define PPAPI_C_PPB_VAR_ARRAY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_ARRAY_INTERFACE_1_0 "PPB_VarArray;1.0"
#define PPB_VAR_ARRAY_INTERFACE PPB_VAR_ARRAY_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_VarArray</code> struct providing
 * a way to interact with array vars.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_VarArray_1_0 {
  /**
   * Creates an array var, i.e., a <code>PP_Var</code> with type set to
   * <code>PP_VARTYPE_ARRAY</code>. The array length is set to 0.
   *
   * @return An empty array var, whose reference count is set to 1 on behalf of
   * the caller.
   */
  struct PP_Var (*Create)(void);
  /**
   * Gets an element from the array.
   *
   * @param[in] array An array var.
   * @param[in] index An index indicating which element to return.
   *
   * @return The element at the specified position. The reference count of the
   * element returned is incremented on behalf of the caller. If
   * <code>index</code> is larger than or equal to the array length, an
   * undefined var is returned.
   */
  struct PP_Var (*Get)(struct PP_Var array, uint32_t index);
  /**
   * Sets the value of an element in the array.
   *
   * @param[in] array An array var.
   * @param[in] index An index indicating which element to modify. If
   * <code>index</code> is larger than or equal to the array length, the length
   * is updated to be <code>index</code> + 1. Any position in the array that
   * hasn't been set before is set to undefined, i.e., <code>PP_Var</code> of
   * type <code>PP_VARTYPE_UNDEFINED</code>.
   * @param[in] value The value to set. The array holds a reference to it on
   * success.
   *
   * @return A <code>PP_Bool</code> indicating whether the operation succeeds.
   */
  PP_Bool (*Set)(struct PP_Var array, uint32_t index, struct PP_Var value);
  /**
   * Gets the array length.
   *
   * @param[in] array An array var.
   *
   * @return The array length.
   */
  uint32_t (*GetLength)(struct PP_Var array);
  /**
   * Sets the array length.
   *
   * @param[in] array An array var.
   * @param[in] length The new array length. If <code>length</code> is smaller
   * than its current value, the array is truncated to the new length; any
   * elements that no longer fit are removed and the references to them will be
   * released. If <code>length</code> is larger than its current value,
   * undefined vars are appended to increase the array to the specified length.
   *
   * @return A <code>PP_Bool</code> indicating whether the operation succeeds.
   */
  PP_Bool (*SetLength)(struct PP_Var array, uint32_t length);
};

typedef struct PPB_VarArray_1_0 PPB_VarArray;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VAR_ARRAY_H_ */

