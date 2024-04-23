// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_webgpu.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/webgpu/callback.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/dawn/include/dawn/webgpu_cpp_print.h"
#include "third_party/dawn/include/dawn/wire/WireClient.h"

namespace video_effects {

VideoEffectsProcessorWebGpu::VideoEffectsProcessorWebGpu(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    base::OnceClosure on_unrecoverable_error)
    : context_provider_(std::move(context_provider)),
      on_unrecoverable_error_(std::move(on_unrecoverable_error)) {
  CHECK(context_provider_);
  CHECK(context_provider_->WebGPUInterface());
}

VideoEffectsProcessorWebGpu::~VideoEffectsProcessorWebGpu() = default;

bool VideoEffectsProcessorWebGpu::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gpu::webgpu::WebGPUInterface* webgpu_interface =
      context_provider_->WebGPUInterface();

  scoped_refptr<gpu::webgpu::APIChannel> webgpu_api_channel =
      webgpu_interface->GetAPIChannel();

  // C++ wrapper for WebGPU requires us to install a proc table globally per
  // process or per thread. Here, we install them per-process.
  dawnProcSetProcs(&dawn::wire::client::GetProcs());

  // Required to create a device. Setting a synthetic token here means that
  // blob cache will be disabled in Dawn, since the mapping that is going to
  // be queried will return an empty string. For more details see
  // `GpuProcessHost::GetIsolationKey()`.
  webgpu_interface->SetWebGPUExecutionContextToken(
      blink::WebGPUExecutionContextToken(blink::DedicatedWorkerToken{}));

  instance_ = wgpu::Instance(webgpu_api_channel->GetWGPUInstance());

  auto* request_adapter_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         WGPURequestAdapterStatus status, WGPUAdapter adapter,
         char const* message) {
        if (processor) {
          processor->OnRequestAdapter(status, adapter, message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  instance_.RequestAdapter(nullptr, request_adapter_callback->UnboundCallback(),
                           request_adapter_callback->AsUserdata());
  EnsureFlush();

  return true;
}

void VideoEffectsProcessorWebGpu::OnRequestAdapter(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != WGPURequestAdapterStatus_Success || !adapter) {
    MaybeCallOnUnrecoverableError();
    return;
  }

  adapter_ = wgpu::Adapter(adapter);

  // TODO(bialpio): Determine the limits based on the incoming video frames.
  wgpu::RequiredLimits limits = {
      .limits = {},
  };

  auto* device_lost_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         WGPUDeviceLostReason reason, char const* message) {
        if (processor) {
          processor->OnDeviceLost(reason, message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  wgpu::DeviceDescriptor descriptor = {
      .label = "VideoEffectsProcessor",
      .requiredLimits = &limits,
      .defaultQueue =
          {
              .label = "VideoEffectsProcessorDefaultQueue",
          },
      .deviceLostCallback = device_lost_callback->UnboundCallback(),
      .deviceLostUserdata = device_lost_callback->AsUserdata(),
  };

  auto* request_device_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         WGPURequestDeviceStatus status, WGPUDevice device,
         char const* message) {
        if (processor) {
          processor->OnRequestDevice(status, device, message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  adapter_.RequestDevice(&descriptor,
                         request_device_callback->UnboundCallback(),
                         request_device_callback->AsUserdata());
  EnsureFlush();
}

void VideoEffectsProcessorWebGpu::OnRequestDevice(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != WGPURequestDeviceStatus_Success || !device) {
    MaybeCallOnUnrecoverableError();
    return;
  }

  device_ = wgpu::Device(device);
  device_.SetUncapturedErrorCallback(&ErrorCallback, nullptr);
  device_.SetLoggingCallback(&LoggingCallback, nullptr);
}

void VideoEffectsProcessorWebGpu::OnDeviceLost(WGPUDeviceLostReason reason,
                                               char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_ = {};

  MaybeCallOnUnrecoverableError();
}

void VideoEffectsProcessorWebGpu::EnsureFlush() {
  if (context_provider_->WebGPUInterface()->EnsureAwaitingFlush()) {
    context_provider_->WebGPUInterface()->FlushAwaitingCommands();
  }
}

void VideoEffectsProcessorWebGpu::MaybeCallOnUnrecoverableError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_unrecoverable_error_) {
    std::move(on_unrecoverable_error_).Run();
  }
}

// static
void VideoEffectsProcessorWebGpu::ErrorCallback(WGPUErrorType type,
                                                char const* message,
                                                void* userdata) {
  LOG(ERROR) << "VideoEffectsProcessor encountered a WebGPU error. type: "
             << type << ", message: " << (message ? message : "(unavailable)");
}

// static
void VideoEffectsProcessorWebGpu::LoggingCallback(WGPULoggingType type,
                                                  char const* message,
                                                  void* userdata) {
  auto log_line = base::StringPrintf(
      "VideoEffectsProcessor received WebGPU log message. message: %s",
      (message ? message : "(unavailable)"));

  switch (type) {
    case WGPULoggingType_Verbose:
      [[fallthrough]];
    case WGPULoggingType_Info:
      VLOG(1) << log_line;
      break;
    case WGPULoggingType_Warning:
      LOG(WARNING) << log_line;
      break;
    case WGPULoggingType_Error:
      LOG(ERROR) << log_line;
      break;
    default:
      VLOG(1) << log_line;
      break;
  }
}

}  // namespace video_effects
