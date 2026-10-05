// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/device_allocator.h"

#include <string_view>

#include "base/logging.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/trivial_model.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

// Returns the memory info to allocate WebNN tensors from, or nullptr for an EP
// that has no usable one.
//
// Which memory to ask for depends on where the EP computes, so it is chosen per
// EP. An integrated GPU wants host-accessible memory, which the CPU and the
// device share instead of copying. A discrete accelerator wants its own memory,
// so tensors stay resident across executions rather than crossing PCIe on every
// one; asking such an EP for host-accessible memory instead yields pinned host
// memory and measurably slows decoding.
//
// Whether the chosen memory is CPU-reachable is a separate question, answered
// by querying the memory info rather than assuming it from the EP.
// TODO(crbug.com/445971854): Use device allocator to create tensors for
// other EPs.
const OrtMemoryInfo* GetMemoryInfo(const OrtApi* ort_api,
                                   const OrtEpDevice* ep_device,
                                   std::string_view ep_name) {
  if (ep_name == kOpenVINOExecutionProvider) {
    return ort_api->EpDevice_MemoryInfo(ep_device,
                                        OrtDeviceMemoryType_HOST_ACCESSIBLE);
  }
  if (ep_name == kNvTensorRTRTXExecutionProvider) {
    return ort_api->EpDevice_MemoryInfo(ep_device, OrtDeviceMemoryType_DEFAULT);
  }
  return nullptr;
}

bool QueryCanAccessOnCpu(OrtAllocator* allocator) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  const OrtMemoryInfo* memory_info = nullptr;
  CHECK_STATUS(ort_api->AllocatorGetInfo(allocator, &memory_info));
  CHECK(memory_info);
  return ort_api->MemoryInfoGetDeviceMemType(memory_info) ==
         OrtDeviceMemoryType_HOST_ACCESSIBLE;
}

}  // namespace

// static
scoped_refptr<DeviceAllocator> DeviceAllocator::Create(
    scoped_refptr<SessionOptions> session_options,
    scoped_refptr<Environment> env) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  const OrtEpDevice* first_selected_device =
      session_options->first_selected_device();

  std::string_view ep_name = ort_api->EpDevice_EpName(first_selected_device);
  const OrtMemoryInfo* memory_info =
      GetMemoryInfo(ort_api, first_selected_device, ep_name);
  if (!memory_info) {
    return nullptr;
  }

  // TODO(crbug.com/519646879): Remove the trivial session once WinML ships
  // ORT 1.27+, which supports getting a shared allocator directly from
  // OrtEnv without creating a session.
  ScopedOrtSessionOptions trivial_session_options =
      CreateTrivialModelSessionOptions(env->get(), first_selected_device);
  ScopedOrtSession trivial_session;
  if (ORT_CALL_FAILED(ort_api->CreateSessionFromArray(
          env->get(), kTrivialModel, sizeof(kTrivialModel),
          trivial_session_options.get(),
          ScopedOrtSession::Receiver(trivial_session).get()))) {
    return nullptr;
  }
  CHECK(trivial_session.get());

  // This allocator is tied to `first_selected_device`, the device the trivial
  // session above was created on.
  ScopedOrtAllocator device_allocator;
  if (ORT_CALL_FAILED(ort_api->CreateAllocator(
          trivial_session.get(), memory_info,
          ScopedOrtAllocator::Receiver(device_allocator).get()))) {
    return nullptr;
  }
  CHECK(device_allocator.get());

  return base::MakeRefCounted<DeviceAllocator>(
      base::PassKey<DeviceAllocator>(), std::move(env),
      std::move(trivial_session), std::move(device_allocator));
}

DeviceAllocator::DeviceAllocator(base::PassKey<DeviceAllocator>,
                                 scoped_refptr<Environment> env,
                                 ScopedOrtSession trivial_session,
                                 ScopedOrtAllocator device_allocator)
    : env_(std::move(env)),
      trivial_session_(std::move(trivial_session)),
      device_allocator_(std::move(device_allocator)),
      can_access_on_cpu_(QueryCanAccessOnCpu(device_allocator_.get())) {}

DeviceAllocator::~DeviceAllocator() = default;

bool DeviceAllocator::ShouldUse(const mojom::TensorInfo& tensor_info) const {
  if (CanAccessOnCpu()) {
    return true;
  }

  return !tensor_info.usage.Has(MLTensorUsageFlags::kRead) &&
         !tensor_info.usage.Has(MLTensorUsageFlags::kWrite);
}

}  // namespace webnn::ort
