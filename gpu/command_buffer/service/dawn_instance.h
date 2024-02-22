// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_

#include <dawn/native/DawnNative.h>
#include <dawn/platform/DawnPlatform.h>

#include <memory>

namespace dawn::platform {
class Platform;
}  // namespace dawn::platform

namespace gpu {

struct GpuPreferences;

namespace webgpu {

enum class SafetyLevel {
  // Enables stable features that are safe to use from unprivileged processes.
  kSafe,
  // Enables experimental features that are safe to use from unprivileged
  // processes.
  kSafeExperimental,
  // Enables all the features, including ones that might not be secure yet and
  // could allow
  // compromising the GPU process.
  kUnsafe,
};

class DawnInstance : public dawn::native::Instance {
 public:
  static std::unique_ptr<DawnInstance> Create(
      dawn::platform::Platform* platform,
      const GpuPreferences& gpu_preferences,
      SafetyLevel safety,
      WGPULoggingCallback logging_callback,
      void* logging_callback_userdata);

 private:
  using dawn::native::Instance::Instance;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_
