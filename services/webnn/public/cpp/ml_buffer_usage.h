// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_ML_BUFFER_USAGE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_ML_BUFFER_USAGE_H_

#include "base/containers/enum_set.h"

namespace webnn {

enum class MLBufferUsageFlags {
  // This buffer may be imported/rented to WebGPU.
  kWebGpuInterop,

  // TODO(crbug.com/343638938): Add more usage flags.

  kMinValue = kWebGpuInterop,
  kMaxValue = kWebGpuInterop,
};

using MLBufferUsage = base::EnumSet<MLBufferUsageFlags,
                                    MLBufferUsageFlags::kMinValue,
                                    MLBufferUsageFlags::kMaxValue>;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_ML_BUFFER_USAGE_H_
