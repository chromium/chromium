// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/webgpu_device.h"

#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/webgpu/callback.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/wire/WireClient.h"

#if MEDIAPIPE_USE_WEBGPU
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_device_registration.h"
#endif

namespace video_effects {

WebGpuDevice::WebGpuDevice(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    DeviceLostCallback device_lost_cb)
    : context_provider_(std::move(context_provider)),
      device_lost_cb_(std::move(device_lost_cb)) {
  CHECK(context_provider_);
  CHECK(context_provider_->WebGPUInterface());
}

WebGpuDevice::~WebGpuDevice() = default;

void WebGpuDevice::Initialize(DeviceCallback device_cb,
                              ErrorCallback error_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!in_progress_);
  in_progress_ = true;

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
      [](base::WeakPtr<WebGpuDevice> self, DeviceCallback device_cb,
         ErrorCallback error_cb, wgpu::RequestAdapterStatus status,
         wgpu::Adapter adapter, wgpu::StringView message) {
        if (self) {
          self->OnRequestAdapter(status, std::move(adapter), message,
                                 std::move(device_cb), std::move(error_cb));
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(device_cb),
      std::move(error_cb));

  instance_.RequestAdapter(nullptr, wgpu::CallbackMode::AllowSpontaneous,
                           request_adapter_callback->UnboundCallback(),
                           request_adapter_callback->AsUserdata());
  EnsureFlush();
}

void WebGpuDevice::OnRequestAdapter(wgpu::RequestAdapterStatus status,
                                    wgpu::Adapter adapter,
                                    std::string_view message,
                                    DeviceCallback device_cb,
                                    ErrorCallback error_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != wgpu::RequestAdapterStatus::Success || !adapter) {
    std::move(error_cb).Run(Error::kFailedToObtainAdapter, message);
    return;
  }

  adapter_ = std::move(adapter);

  // TODO(bialpio): Determine the limits based on the incoming video frames.
  wgpu::RequiredLimits limits = {
      .limits = {},
  };

  auto* device_lost_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<WebGpuDevice> self, const wgpu::Device& device,
         wgpu::DeviceLostReason reason, wgpu::StringView message) {
        if (self) {
          self->OnDeviceLost(device, reason, message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  wgpu::DeviceDescriptor descriptor;
  descriptor.label = "VideoEffectsProcessor";
  descriptor.requiredLimits = &limits;
  descriptor.defaultQueue = {
      .label = "VideoEffectsProcessorDefaultQueue",
  };
  descriptor.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
                                   device_lost_callback->UnboundCallback(),
                                   device_lost_callback->AsUserdata());

  descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
        DVLOG(1) << "wgpu::ErrorType = " << base::to_underlying(type) << "; "
                 << std::string_view(message);
      });

  auto* request_device_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<WebGpuDevice> self, DeviceCallback device_cb,
         ErrorCallback error_cb, wgpu::RequestDeviceStatus status,
         wgpu::Device device, wgpu::StringView message) {
        if (self) {
          self->OnRequestDevice(status, std::move(device), message,
                                std::move(device_cb), std::move(error_cb));
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(device_cb),
      std::move(error_cb));
  adapter_.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                         request_device_callback->UnboundCallback(),
                         request_device_callback->AsUserdata());
  EnsureFlush();
}

void WebGpuDevice::OnRequestDevice(wgpu::RequestDeviceStatus status,
                                   wgpu::Device device,
                                   std::string_view message,
                                   DeviceCallback device_cb,
                                   ErrorCallback error_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != wgpu::RequestDeviceStatus::Success || !device) {
    std::move(error_cb).Run(Error::kFailedToObtainDevice, message);
    return;
  }

  device.SetLoggingCallback(&LoggingCallback, nullptr);

#if MEDIAPIPE_USE_WEBGPU
  mediapipe::WebGpuDeviceRegistration::GetInstance().RegisterWebGpuDevice(
      device);
#endif

  std::move(device_cb).Run(std::move(device));
}

void WebGpuDevice::OnDeviceLost(const wgpu::Device& device,
                                wgpu::DeviceLostReason reason,
                                std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if MEDIAPIPE_USE_WEBGPU
  mediapipe::WebGpuDeviceRegistration::GetInstance().UnRegisterWebGpuDevice();
#endif
  if (device_lost_cb_) {
    std::move(device_lost_cb_).Run(reason, message);
  }
}

// static
void WebGpuDevice::LoggingCallback(WGPULoggingType type,
                                   WGPUStringView message,
                                   void* userdata) {
  std::string_view message_str{message.data, message.length};
  switch (type) {
    case WGPULoggingType_Verbose:
    case WGPULoggingType_Info:
      DVLOG(1) << message_str;
      break;
    case WGPULoggingType_Warning:
      LOG(WARNING) << message_str;
      break;
    case WGPULoggingType_Error:
      LOG(ERROR) << message_str;
      break;
    default:
      DVLOG(1) << message_str;
      break;
  }
}

void WebGpuDevice::EnsureFlush() {
  if (context_provider_->WebGPUInterface()->EnsureAwaitingFlush()) {
    context_provider_->WebGPUInterface()->FlushAwaitingCommands();
  }
}

}  // namespace video_effects
