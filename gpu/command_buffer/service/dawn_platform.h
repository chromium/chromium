// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_

#include <dawn_platform/DawnPlatform.h>

namespace gpu {
namespace webgpu {

class DawnPlatform : public dawn_platform::Platform {
 public:
  DawnPlatform();
  ~DawnPlatform() override;

  const unsigned char* GetTraceCategoryEnabledFlag(
      dawn_platform::TraceCategory category) override;

  double MonotonicallyIncreasingTime() override;

  uint64_t AddTraceEvent(char phase,
                         const unsigned char* category_group_enabled,
                         const char* name,
                         uint64_t id,
                         double timestamp,
                         int num_args,
                         const char** arg_names,
                         const unsigned char* arg_types,
                         const uint64_t* arg_values,
                         unsigned char flags) override;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_
