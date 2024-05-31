// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace viz {
class ContextProviderCommandBuffer;
}

namespace video_effects {

// Companion to `VideoEffectsProcessorImpl`. Initializes the WebGPU API that'll
// subsequently be used for post-processing the incoming video frames.
class VideoEffectsProcessorWebGpu {
 public:
  // `on_unrecoverable_error` will be called when this instance encounters an
  // error it's unable to recover from. It is guaranteed that the callback will
  // not be called synchronously from the ctor and from `Initialize()` method.
  explicit VideoEffectsProcessorWebGpu(
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      base::OnceClosure on_unrecoverable_error);
  ~VideoEffectsProcessorWebGpu();

  VideoEffectsProcessorWebGpu(const VideoEffectsProcessorWebGpu& other) =
      delete;
  VideoEffectsProcessorWebGpu& operator=(
      const VideoEffectsProcessorWebGpu& other) = delete;

  // Returns `true` if the synchronous part of initialization has completed.
  // This does not mean that the processor is fully initialized - WebGPU
  // initialization may still proceed asynchronously.
  bool Initialize();

 private:
  // Ensures that awaiting WebGPUInterface commands are flushed.
  void EnsureFlush();

  // Calls `on_unrecoverable_error_` if it's set.
  void MaybeCallOnUnrecoverableError();

  void OnRequestAdapter(wgpu::RequestAdapterStatus status,
                        wgpu::Adapter adapter,
                        char const* message);

  void OnRequestDevice(wgpu::RequestDeviceStatus status,
                       wgpu::Device device,
                       char const* message);

  void OnDeviceLost(WGPUDeviceLostReason reason, char const* message);

  static void ErrorCallback(WGPUErrorType type,
                            char const* message,
                            void* userdata);

  static void LoggingCallback(WGPULoggingType type,
                              char const* message,
                              void* userdata);

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;

  base::OnceClosure on_unrecoverable_error_;

  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsProcessorWebGpu> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_
