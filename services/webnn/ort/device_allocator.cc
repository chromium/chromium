// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/device_allocator.h"

#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

// The 4096 alignment is needed by Intel NPU hardware on OpenVINO.
// According to:
// https://github.com/openvinotoolkit/openvino/blob/14bf903270f78592c4572c88c936bdf0120e2fb9/src/plugins/intel_npu/src/utils/include/intel_npu/utils/utils.hpp#L15
constexpr size_t kIntelNpuStandardPageSize = 4096;

// Creates memory info for a specific EP.
// Returns an invalid memory info if the device allocator is not supported for
// the EP.
ScopedOrtMemoryInfo CreateMemoryInfo(const OrtApi* ort_api,
                                     base::cstring_view ep_name) {
  ScopedOrtMemoryInfo memory_info;
  if (ep_name == kOpenVINOExecutionProvider) {
    // "OpenVINO_shared" memory info represents shared CPU memory for OpenVINO
    // EP.
    CHECK_STATUS(ort_api->CreateMemoryInfo_V2(
        "OpenVINO_shared", OrtMemoryInfoDeviceType_CPU, /*vendor_id*/ 0x8086,
        /*device_id*/ 0, OrtDeviceMemoryType_HOST_ACCESSIBLE,
        /*alignment*/ kIntelNpuStandardPageSize, OrtDeviceAllocator,
        ScopedOrtMemoryInfo::Receiver(memory_info).get()));
    CHECK(memory_info.get());
  } else if (ep_name == kWebGpuExecutionProvider) {
    CHECK_STATUS(ort_api->CreateMemoryInfo(
        "WebGPU_Buffer", OrtDeviceAllocator, /*id*/ 0, OrtMemTypeDefault,
        ScopedOrtMemoryInfo::Receiver(memory_info).get()));
    CHECK(memory_info.get());
  } else {
    LOG(WARNING) << "[WebNN] Device allocator is not supported for " << ep_name;
  }

  return memory_info;
}

}  // namespace

// static
scoped_refptr<DeviceAllocator> DeviceAllocator::Create(
    mojom::Device device_type,
    const OrtSessionOptions* session_options,
    scoped_refptr<Environment> env) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  base::span<const OrtEpDevice* const> registered_ep_devices =
      env->GetRegisteredEpDevices();
  std::vector<const OrtEpDevice*> selected_ep_devices =
      Environment::SelectEpDevices(registered_ep_devices, device_type);
  if (selected_ep_devices.empty()) {
    LOG(ERROR)
        << "[WebNN] No suitable EP device found for creating DeviceAllocator.";
    return nullptr;
  }

  const OrtEpDevice* first_selected_device = selected_ep_devices.front();
  CHECK(first_selected_device);

  const char* ep_name = ort_api->EpDevice_EpName(first_selected_device);
  // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
  ScopedOrtMemoryInfo memory_info =
      CreateMemoryInfo(ort_api, UNSAFE_BUFFERS(base::cstring_view(ep_name)));
  if (!memory_info.is_valid()) {
    return nullptr;
  }

  // Trivial ONNX model that returns a single float constant.
  // Used to create a trivial session for obtaining a device allocator.
  // Model bytes are copied from onnxruntime-genai:
  // https://github.com/microsoft/onnxruntime-genai/blob/ded6e97789ca718d76ce58bba4a2b483b10045ee/src/models/model.cpp#L743
  static constexpr uint8_t kTrivialModel[] = {
      0x08, 0x0a, 0x12, 0x01, 0x61, 0x3a, 0x53, 0x0a, 0x38, 0x12, 0x06, 0x76,
      0x61, 0x6c, 0x75, 0x65, 0x73, 0x22, 0x08, 0x43, 0x6f, 0x6e, 0x73, 0x74,
      0x61, 0x6e, 0x74, 0x2a, 0x24, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
      0x2a, 0x18, 0x08, 0x01, 0x10, 0x01, 0x42, 0x0c, 0x63, 0x6f, 0x6e, 0x73,
      0x74, 0x5f, 0x74, 0x65, 0x6e, 0x73, 0x6f, 0x72, 0x4a, 0x04, 0x00, 0x00,
      0x00, 0x00, 0xa0, 0x01, 0x04, 0x12, 0x01, 0x62, 0x62, 0x14, 0x0a, 0x06,
      0x76, 0x61, 0x6c, 0x75, 0x65, 0x73, 0x12, 0x0a, 0x0a, 0x08, 0x08, 0x01,
      0x12, 0x04, 0x0a, 0x02, 0x08, 0x01, 0x42, 0x04, 0x0a, 0x00, 0x10, 0x15};

  ScopedOrtSession trivial_session;
  CHECK_STATUS(ort_api->CreateSessionFromArray(
      env->get(), kTrivialModel, sizeof(kTrivialModel), session_options,
      ScopedOrtSession::Receiver(trivial_session).get()));
  CHECK(trivial_session.get());

  ScopedOrtAllocator device_allocator;
  CHECK_STATUS(ort_api->CreateAllocator(
      trivial_session.get(), memory_info.get(),
      ScopedOrtAllocator::Receiver(device_allocator).get()));
  CHECK(device_allocator.get());

  // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
  return base::MakeRefCounted<DeviceAllocator>(
      base::PassKey<DeviceAllocator>(), std::move(trivial_session),
      std::move(device_allocator), UNSAFE_BUFFERS(base::cstring_view(ep_name)));
}

DeviceAllocator::DeviceAllocator(base::PassKey<DeviceAllocator>,
                                 ScopedOrtSession trivial_session,
                                 ScopedOrtAllocator device_allocator,
                                 base::cstring_view ep_name)
    : trivial_session_(std::move(trivial_session)),
      device_allocator_(std::move(device_allocator)),
      ep_name_(ep_name) {}

DeviceAllocator::~DeviceAllocator() = default;

bool DeviceAllocator::ShouldUse(const mojom::TensorInfoPtr& tensor_info) const {
  // Since the WebGPU EP does not allow clients to access underlying tensors
  // directly, only use it when WebNN developers do not need to access the
  // underlying data.
  if (ep_name_ == kWebGpuExecutionProvider &&
      (tensor_info->usage.Has(MLTensorUsageFlags::kRead) ||
       tensor_info->usage.Has(MLTensorUsageFlags::kWrite))) {
    return false;
  }

  return true;
}

}  // namespace webnn::ort
