// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_UMA_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_UMA_PRIVATE_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class CompletionCallback;

class UMAPrivate {
 public:
  UMAPrivate();
  explicit UMAPrivate(const InstanceHandle& instance);
  ~UMAPrivate();

  static bool IsAvailable();

  void HistogramCustomTimes(const std::string& name,
                            int64_t sample,
                            int64_t min,
                            int64_t max,
                            uint32_t bucket_count);

  void HistogramCustomCounts(const std::string& name,
                             int32_t sample,
                             int32_t min,
                             int32_t max,
                             uint32_t bucket_count);

  void HistogramEnumeration(const std::string& name,
                            int32_t sample,
                            int32_t boundary_value);

  int32_t IsCrashReportingEnabled(const CompletionCallback& cc);

 private:
  PP_Instance instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_UMA_PRIVATE_H_
