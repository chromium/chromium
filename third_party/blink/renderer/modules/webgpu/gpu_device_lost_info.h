// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GPUDeviceLostInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUDeviceLostInfo(const wgpu::DeviceLostReason reason,
                             const String& message);

  GPUDeviceLostInfo(const GPUDeviceLostInfo&) = delete;
  GPUDeviceLostInfo& operator=(const GPUDeviceLostInfo&) = delete;

  // gpu_device_lost_info.idl
  const String& reason() const;
  const String& message() const;

 private:
  String reason_;
  String message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_
