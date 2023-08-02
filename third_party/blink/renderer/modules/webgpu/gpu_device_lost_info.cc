// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"

#include "base/notreached.h"

namespace blink {

GPUDeviceLostInfo::GPUDeviceLostInfo(const WGPUDeviceLostReason reason,
                                     const String& message) {
  switch (reason) {
    case WGPUDeviceLostReason_Undefined:
      reason_ = "unknown";
      break;
    case WGPUDeviceLostReason_Destroyed:
      reason_ = "destroyed";
      break;
    default:
      // If this is hit, Dawn gave us a reason we haven't implemented here yet.
      NOTREACHED();
      reason_ = "unknown";
      break;
  }
  message_ = message;
}

const String& GPUDeviceLostInfo::reason() const {
  return reason_;
}

const String& GPUDeviceLostInfo::message() const {
  return message_;
}

}  // namespace blink
