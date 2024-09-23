// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"

namespace blink {

GPUDeviceLostInfo::GPUDeviceLostInfo(const wgpu::DeviceLostReason reason,
                                     const String& message) {
  switch (reason) {
    case wgpu::DeviceLostReason::Unknown:
    case wgpu::DeviceLostReason::InstanceDropped:
    case wgpu::DeviceLostReason::FailedCreation:
      reason_ = "unknown";
      break;
    case wgpu::DeviceLostReason::Destroyed:
      reason_ = "destroyed";
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
