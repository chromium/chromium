// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/device_allocator.h"

#include <string_view>

#include "base/logging.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/trivial_model.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

// Returns the host-accessible memory info from the EP device. Currently only
// OpenVINO EP is supported. Returns nullptr for unsupported EPs.
const OrtMemoryInfo* GetMemoryInfo(const OrtApi* ort_api,
                                   const OrtEpDevice* ep_device,
                                   std::string_view ep_name) {
  if (ep_name == kOpenVINOExecutionProvider) {
    return ort_api->EpDevice_MemoryInfo(ep_device,
                                        OrtDeviceMemoryType_HOST_ACCESSIBLE);
  }
  return nullptr;
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
  ScopedOrtSessionOptions cloned_session_options;
  CHECK_STATUS(ort_api->CloneSessionOptions(
      session_options->get(),
      ScopedOrtSessionOptions::Receiver(cloned_session_options).get()));
  // Model compilation is normally disabled in the GPU process when
  // `features::kWebNNCompilerProcess` is active, but `kTrivialModel` contains
  // uncompiled operators and would fail to compile without it, so re-enable it
  // here temporarily.
  CHECK_STATUS(ort_api->AddSessionConfigEntry(
      cloned_session_options.get(), kOrtSessionOptionsDisableModelCompile,
      "0"));
  ScopedOrtSession trivial_session;
  CHECK_STATUS(ort_api->CreateSessionFromArray(
      env->get(), kTrivialModel, sizeof(kTrivialModel),
      cloned_session_options.get(),
      ScopedOrtSession::Receiver(trivial_session).get()));
  CHECK(trivial_session.get());

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
      device_allocator_(std::move(device_allocator)) {}

DeviceAllocator::~DeviceAllocator() = default;

}  // namespace webnn::ort
