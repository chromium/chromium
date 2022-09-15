/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_time.idl modified Fri May 10 15:48:42 2013. */

#ifndef PPAPI_C_PP_TIME_H_
#define PPAPI_C_PP_TIME_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines time, time ticks and time delta types.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * The <code>PP_Time</code> type represents the "wall clock time" according
 * to the browser and is defined as the number of seconds since the Epoch
 * (00:00:00 UTC, January 1, 1970).
 */
typedef double PP_Time;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Time, 8);

/**
 * A <code>PP_TimeTicks</code> value represents time ticks which are measured
 * in seconds and are used for indicating the time that certain messages were
 * received. In contrast to <code>PP_Time</code>, <code>PP_TimeTicks</code>
 * does not correspond to any actual wall clock time and will not change
 * discontinuously if the user changes their computer clock.
 *
 * The units are in seconds, but are not measured relative to any particular
 * epoch, so the most you can do is compare two values.
 */
typedef double PP_TimeTicks;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_TimeTicks, 8);

/**
 * A <code>PP_TimeDelta</code> value represents a duration of time which is
 * measured in seconds.
 */
typedef double PP_TimeDelta;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_TimeDelta, 8);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_TIME_H_ */

