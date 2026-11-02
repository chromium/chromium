// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_

#include <dawn/native/DawnNative.h>
#include <dawn/platform/DawnPlatform.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/dawn_platform.h"

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

// Wraps a dawn::native::Instance and owns the DawnPlatform that is used by it.
// The underlying Dawn instance may outlive this wrapper (e.g. via references
// held by wgpu objects), so the platform is disconnected from the Dawn instance
// before it is destroyed.
class DawnInstance : public dawn::native::Instance,
                     public base::RefCountedThreadSafe<DawnInstance> {
 public:
  // Creates a DawnInstance configured for servicing WebGPU contexts. The
  // safety level is derived from `gpu_preferences`.
  static scoped_refptr<DawnInstance> CreateForWebGPU(
      const GpuPreferences& gpu_preferences);

  // Creates a DawnInstance configured for Graphite. Since access to this
  // instance doesn't leave the GPU process, unsafe APIs are always allowed.
  static scoped_refptr<DawnInstance> CreateForGraphite(
      const GpuPreferences& gpu_preferences,
      gl::ProgressReporter* progress_reporter,
      dawn::native::DawnInstanceDescriptor* dawn_instance_descriptor = nullptr);

  DawnPlatform& platform() const { return *platform_; }

  // The safety level that this instance was created with.
  SafetyLevel safety_level() const { return safety_level_; }

 private:
  friend class base::RefCountedThreadSafe<DawnInstance>;

  static scoped_refptr<DawnInstance> Create(
      std::unique_ptr<DawnPlatform> platform,
      const GpuPreferences& gpu_preferences,
      SafetyLevel safety,
      dawn::native::DawnInstanceDescriptor* dawn_instance_descriptor);

  DawnInstance(const WGPUInstanceDescriptor* desc,
               std::unique_ptr<DawnPlatform> platform,
               SafetyLevel safety_level);
  ~DawnInstance();

  const std::unique_ptr<DawnPlatform> platform_;
  const SafetyLevel safety_level_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_INSTANCE_H_
