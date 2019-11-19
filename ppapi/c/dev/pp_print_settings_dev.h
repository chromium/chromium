/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/pp_print_settings_dev.idl modified Tue Sep  3 14:00:26 2019. */

#ifndef PPAPI_C_DEV_PP_PRINT_SETTINGS_DEV_H_
#define PPAPI_C_DEV_PP_PRINT_SETTINGS_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the struct for PrintSettings.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_PRINTORIENTATION_NORMAL = 0,
  PP_PRINTORIENTATION_ROTATED_90_CW = 1,
  PP_PRINTORIENTATION_ROTATED_180 = 2,
  PP_PRINTORIENTATION_ROTATED_90_CCW = 3,
  PP_PRINTORIENTATION_ROTATED_LAST = PP_PRINTORIENTATION_ROTATED_90_CCW
} PP_PrintOrientation_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOrientation_Dev, 4);

typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER = 1u << 0,
  PP_PRINTOUTPUTFORMAT_PDF = 1u << 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT = 1u << 2,
  PP_PRINTOUTPUTFORMAT_EMF = 1u << 3
} PP_PrintOutputFormat_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOutputFormat_Dev, 4);

typedef enum {
  PP_PRINTSCALINGOPTION_NONE = 0,
  PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA = 1,
  PP_PRINTSCALINGOPTION_SOURCE_SIZE = 2,
  PP_PRINTSCALINGOPTION_FIT_TO_PAPER = 3,
  PP_PRINTSCALINGOPTION_LAST = PP_PRINTSCALINGOPTION_FIT_TO_PAPER
} PP_PrintScalingOption_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintScalingOption_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_PrintSettings_Dev {
  /** This is the size of the printable area in points (1/72 of an inch). */
  struct PP_Rect printable_area;
  struct PP_Rect content_area;
  struct PP_Size paper_size;
  int32_t dpi;
  PP_PrintOrientation_Dev orientation;
  PP_PrintScalingOption_Dev print_scaling_option;
  PP_Bool grayscale;
  /** Note that Chrome currently only supports PDF printing. */
  PP_PrintOutputFormat_Dev format;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PrintSettings_Dev, 60);
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PP_PRINT_SETTINGS_DEV_H_ */

