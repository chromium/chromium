// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_UMA_SINGLETON_API_H_
#define PPAPI_THUNK_PPB_UMA_SINGLETON_API_H_

#include <stdint.h>

#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_UMA_Singleton_API {
 public:
  virtual ~PPB_UMA_Singleton_API() {}

  virtual void HistogramCustomTimes(PP_Instance instance,
                                    struct PP_Var name,
                                    int64_t sample,
                                    int64_t min,
                                    int64_t max,
                                    uint32_t bucket_count) = 0;

  virtual void HistogramCustomCounts(PP_Instance instance,
                                     struct PP_Var name,
                                     int32_t sample,
                                     int32_t min,
                                     int32_t max,
                                     uint32_t bucket_count) = 0;

  virtual void HistogramEnumeration(PP_Instance instance,
                                    struct PP_Var name,
                                    int32_t sample,
                                    int32_t boundary_value) = 0;

  virtual int32_t IsCrashReportingEnabled(
      PP_Instance instance,
      scoped_refptr<TrackedCallback> cc) = 0;

  static const SingletonResourceID kSingletonResourceID = UMA_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_UMA_SINGLETON_API_H_
