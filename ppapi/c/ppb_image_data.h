/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_image_data.idl modified Tue Nov 13 08:48:25 2012. */

#ifndef PPAPI_C_PPB_IMAGE_DATA_H_
#define PPAPI_C_PPB_IMAGE_DATA_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_IMAGEDATA_INTERFACE_1_0 "PPB_ImageData;1.0"
#define PPB_IMAGEDATA_INTERFACE PPB_IMAGEDATA_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_ImageData</code> struct for determining how
 * a browser handles image data.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_ImageDataFormat</code> is an enumeration of the different types of
 * image data formats.
 *
 * The third part of each enumeration value describes the memory layout from
 * the lowest address to the highest. For example, BGRA means the B component
 * is stored in the lowest address, no matter what endianness the platform is
 * using.
 *
 * The PREMUL suffix implies pre-multiplied alpha is used. In this mode, the
 * red, green and blue color components of the pixel data supplied to an image
 * data should be pre-multiplied by their alpha value. For example: starting
 * with floating point color components, here is how to convert them to 8-bit
 * premultiplied components for image data:
 *
 * ...components of a pixel, floats ranging from 0 to 1...
 * <code>float red = 1.0f;</code>
 * <code>float green = 0.50f;</code>
 * <code>float blue = 0.0f;</code>
 * <code>float alpha = 0.75f;</code>
 * ...components for image data are 8-bit values ranging from 0 to 255...
 * <code>uint8_t image_data_red_premul = (uint8_t)(red * alpha * 255.0f);
 * </code>
 * <code>uint8_t image_data_green_premul = (uint8_t)(green * alpha * 255.0f);
 * </code>
 * <code>uint8_t image_data_blue_premul = (uint8_t)(blue * alpha * 255.0f);
 * </code>
 * <code>uint8_t image_data_alpha_premul = (uint8_t)(alpha * 255.0f);</code>
 *
 * <strong>Note:</strong> The resulting pre-multiplied red, green and blue
 * components should not be greater than the alpha value.
 */
typedef enum {
  PP_IMAGEDATAFORMAT_BGRA_PREMUL,
  PP_IMAGEDATAFORMAT_RGBA_PREMUL,
  PP_IMAGEDATAFORMAT_LAST = PP_IMAGEDATAFORMAT_RGBA_PREMUL
} PP_ImageDataFormat;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_ImageDataFormat, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * The <code>PP_ImageDataDesc</code> structure represents a description of
 * image data.
 */
struct PP_ImageDataDesc {
  /**
   * This value represents one of the image data types in the
   * <code>PP_ImageDataFormat</code> enum.
   */
  PP_ImageDataFormat format;
  /** This value represents the size of the bitmap in pixels. */
  struct PP_Size size;
  /**
   * This value represents the row width in bytes. This may be different than
   * width * 4 since there may be padding at the end of the lines.
   */
  int32_t stride;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_ImageDataDesc, 16);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_ImageData</code> interface contains pointers to several
 * functions for determining the browser's treatment of image data.
 */
struct PPB_ImageData_1_0 {
  /**
   * GetNativeImageDataFormat() returns the browser's preferred format for
   * image data. The browser uses this format internally for painting. Other
   * formats may require internal conversions to paint or may have additional
   * restrictions depending on the function.
   *
   * @return A <code>PP_ImageDataFormat</code> containing the preferred format.
   */
  PP_ImageDataFormat (*GetNativeImageDataFormat)(void);
  /**
   * IsImageDataFormatSupported() determines if the given image data format is
   * supported by the browser. Note: <code>PP_IMAGEDATAFORMAT_BGRA_PREMUL</code>
   * and <code>PP_IMAGEDATAFORMAT_RGBA_PREMUL</code> formats are always
   * supported. Other image formats do not make this guarantee, and should be
   * checked first with IsImageDataFormatSupported() before using.
   *
   * @param[in] format The image data format.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * image data format is supported by the browser.
   */
  PP_Bool (*IsImageDataFormatSupported)(PP_ImageDataFormat format);
  /**
   * Create() allocates an image data resource with the given format and size.
   *
   * For security reasons, if uninitialized, the bitmap will not contain random
   * memory, but may contain data from a previous image produced by the same
   * module if the bitmap was cached and re-used.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] format The desired image data format.
   * @param[in] size A pointer to a <code>PP_Size</code> containing the image
   * size.
   * @param[in] init_to_zero A <code>PP_Bool</code> to determine transparency
   * at creation.
   * Set the <code>init_to_zero</code> flag if you want the bitmap initialized
   * to transparent during the creation process. If this flag is not set, the
   * current contents of the bitmap will be undefined, and the module should
   * be sure to set all the pixels.
   *
   * @return A <code>PP_Resource</code> with a nonzero ID on success or zero on
   * failure. Failure means the instance, image size, or format was invalid.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_ImageDataFormat format,
                        const struct PP_Size* size,
                        PP_Bool init_to_zero);
  /**
   * IsImageData() determines if a given resource is image data.
   *
   * @param[in] image_data A <code>PP_Resource</code> corresponding to image
   * data.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is an image data or <code>PP_FALSE</code> if the resource is
   * invalid or some type other than image data.
   */
  PP_Bool (*IsImageData)(PP_Resource image_data);
  /**
   * Describe() computes the description of the
   * image data.
   *
   * @param[in] image_data A <code>PP_Resource</code> corresponding to image
   * data.
   * @param[in,out] desc A pointer to a <code>PP_ImageDataDesc</code>
   * containing the description.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> on success or
   * <code>PP_FALSE</code> if the resource is not an image data. On
   * <code>PP_FALSE</code>, the <code>desc</code> structure will be filled
   * with 0.
   */
  PP_Bool (*Describe)(PP_Resource image_data, struct PP_ImageDataDesc* desc);
  /**
   * Map() maps an image data into the module address space.
   *
   * @param[in] image_data A <code>PP_Resource</code> corresponding to image
   * data.
   *
   * @return A pointer to the beginning of the data.
   */
  void* (*Map)(PP_Resource image_data);
  /**
   * Unmap is a pointer to a function that unmaps an image data from the module
   * address space.
   *
   * @param[in] image_data A <code>PP_Resource</code> corresponding to image
   * data.
   */
  void (*Unmap)(PP_Resource image_data);
};

typedef struct PPB_ImageData_1_0 PPB_ImageData;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_IMAGE_DATA_H_ */

