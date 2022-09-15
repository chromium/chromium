/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_size.idl modified Tue Jun  3 12:31:20 2014. */

#ifndef PPAPI_C_PP_SIZE_H_
#define PPAPI_C_PP_SIZE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the width and height of a 2D rectangle.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * The <code>PP_Size</code> struct contains the size of a 2D rectangle.
 */
struct PP_Size {
  /** This value represents the width of the rectangle. */
  int32_t width;
  /** This value represents the height of the rectangle. */
  int32_t height;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Size, 8);

/**
 * The <code>PP_FloatSize</code> struct contains the size of a 2D rectangle.
 */
struct PP_FloatSize {
  /** This value represents the width of the rectangle. */
  float width;
  /** This value represents the height of the rectangle. */
  float height;
};
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakeSize() creates a <code>PP_Size</code> given a width and height as
 * int32_t values.
 *
 * @param[in] w An int32_t value representing a width.
 * @param[in] h An int32_t value representing a height.
 *
 * @return A <code>PP_Size</code> structure.
 */
PP_INLINE struct PP_Size PP_MakeSize(int32_t w, int32_t h) {
  struct PP_Size ret;
  ret.width = w;
  ret.height = h;
  return ret;
}

/**
 * PP_MakeFloatSize() creates a <code>PP_FloatSize</code> given a
 * width and height as float values.
 *
 * @param[in] w An float value representing a width.
 * @param[in] h An float value representing a height.
 *
 * @return A <code>PP_FloatSize</code> structure.
 */
PP_INLINE struct PP_FloatSize PP_MakeFloatSize(float w, float h) {
  struct PP_FloatSize ret;
  ret.width = w;
  ret.height = h;
  return ret;
}
/**
 * @}
 */
#endif  /* PPAPI_C_PP_SIZE_H_ */

