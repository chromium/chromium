// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_ML_TENSOR_USAGE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_ML_TENSOR_USAGE_H_

#include "base/containers/enum_set.h"

namespace webnn {

enum class MLTensorUsageFlags {
  // This buffer may be imported/rented to WebGPU.
  kWebGpuInterop,

  // This buffer can be used with readBuffer().
  kReadFrom,

  // This buffer can be used with writeBuffer().
  kWriteTo,

  kMinValue = kWebGpuInterop,
  kMaxValue = kWriteTo,
};

using MLTensorUsage = base::EnumSet<MLTensorUsageFlags,
                                    MLTensorUsageFlags::kMinValue,
                                    MLTensorUsageFlags::kMaxValue>;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_ML_TENSOR_USAGE_H_
