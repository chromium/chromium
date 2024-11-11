// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_WEBGPU_DEVICE_H_
#define SERVICES_VIDEO_EFFECTS_WEBGPU_DEVICE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace video_effects {

// Initializes and holds a wgpu::Device for the Video Effects Service.
// It is expected that only one instance exists for the lifetime of the service,
// and it is shared among VideoEffectProcessors.
class WebGpuDevice final {
 public:
  enum class Error {
    kFailedToObtainAdapter = 0,
    kFailedToObtainDevice,
    kUncapturedError,
  };

  using DeviceCallback = base::OnceCallback<void(wgpu::Device)>;
  using ErrorCallback = base::OnceCallback<void(Error, std::string_view)>;
  using DeviceLostCallback =
      base::OnceCallback<void(wgpu::DeviceLostReason, std::string_view)>;

  // `context_provider` provides access to the GPU context (command buffer).
  // `device_lost_cb` is invoked if the GPU context was lost with the reason and
  // message.  The wgpu::Device is unusable after `device_lost_cb` is run.
  WebGpuDevice(
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      DeviceLostCallback device_lost_cb);
  ~WebGpuDevice();
  WebGpuDevice(const WebGpuDevice&) = delete;
  WebGpuDevice& operator=(const WebGpuDevice&) = delete;

  // Initializes the wgpu::Device.  `device_cb` is invoked with the device if it
  // was created successfully.  `error_cb` is invoked if the device could not be
  // created with the error type and message.  Either `device_cb` or `error_cb`
  // is guaranteed to be called, and `Initialize` must be called exactly once.
  void Initialize(DeviceCallback device_cb, ErrorCallback error_cb);

 private:
  void OnRequestAdapter(wgpu::RequestAdapterStatus status,
                        wgpu::Adapter adapter,
                        std::string_view message,
                        DeviceCallback device_cb,
                        ErrorCallback error_cb);

  void OnRequestDevice(wgpu::RequestDeviceStatus status,
                       wgpu::Device device,
                       std::string_view message,
                       DeviceCallback device_cb,
                       ErrorCallback error_cb);

  void OnDeviceLost(const wgpu::Device& device,
                    wgpu::DeviceLostReason reason,
                    std::string_view message);

  static void LoggingCallback(WGPULoggingType type,
                              WGPUStringView message,
                              void* userdata);

  void EnsureFlush();

  // Set to true when Initialize() is called.
  bool in_progress_ = false;

  wgpu::Instance instance_;
  wgpu::Adapter adapter_;

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  DeviceLostCallback device_lost_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<WebGpuDevice> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_WEBGPU_DEVICE_H_
