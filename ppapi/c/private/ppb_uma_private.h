/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_uma_private.idl modified Fri Mar 14 16:59:33 2014. */

#ifndef PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_UMA_PRIVATE_INTERFACE_0_3 "PPB_UMA_Private;0.3"
#define PPB_UMA_PRIVATE_INTERFACE PPB_UMA_PRIVATE_INTERFACE_0_3

/**
 * @file
 * This file defines the <code>PPB_UMA_Private</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Contains functions for plugins to report UMA usage stats.
 */
struct PPB_UMA_Private_0_3 {
  /**
   * HistogramCustomTimes is a pointer to a function which records a time
   * sample given in milliseconds in the histogram given by |name|, possibly
   * creating the histogram if it does not exist.
   */
  void (*HistogramCustomTimes)(PP_Instance instance,
                               struct PP_Var name,
                               int64_t sample,
                               int64_t min,
                               int64_t max,
                               uint32_t bucket_count);
  /**
   * HistogramCustomCounts is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.
   */
  void (*HistogramCustomCounts)(PP_Instance instance,
                                struct PP_Var name,
                                int32_t sample,
                                int32_t min,
                                int32_t max,
                                uint32_t bucket_count);
  /**
   * HistogramEnumeration is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.  The sample represents a value in an enumeration bounded
   * by |boundary_value|, that is, sample < boundary_value always.
   */
  void (*HistogramEnumeration)(PP_Instance instance,
                               struct PP_Var name,
                               int32_t sample,
                               int32_t boundary_value);
  /**
   * IsCrashReportingEnabled returns PP_OK to the completion callback to
   * indicate that the current user has opted-in to crash reporting, or
   * PP_ERROR_* on failure or when a user has not opted-in.  This can be used to
   * gate other reporting processes such as analytics and crash reporting.
   */
  int32_t (*IsCrashReportingEnabled)(PP_Instance instance,
                                     struct PP_CompletionCallback callback);
};

typedef struct PPB_UMA_Private_0_3 PPB_UMA_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_ */

