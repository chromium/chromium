// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TIME_CONVERSION_H_
#define PPAPI_SHARED_IMPL_TIME_CONVERSION_H_

#include "base/time/time.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

PPAPI_SHARED_EXPORT PP_Time TimeToPPTime(base::Time t);
PPAPI_SHARED_EXPORT base::Time PPTimeToTime(PP_Time t);

PPAPI_SHARED_EXPORT PP_TimeTicks TimeTicksToPPTimeTicks(base::TimeTicks t);

// Gets the local time zone offset for a given time. This works in the plugin
// process only on Windows (the sandbox prevents this from working properly on
// other platforms).
PPAPI_SHARED_EXPORT double PPGetLocalTimeZoneOffset(const base::Time& time);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TIME_CONVERSION_H_
