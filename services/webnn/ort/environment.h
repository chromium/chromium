// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ENVIRONMENT_H_
#define SERVICES_WEBNN_ORT_ENVIRONMENT_H_

#include <string>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/cstring_view.h"
#include "base/synchronization/lock.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "gpu/config/gpu_info.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

// A wrapper of `OrtEnv` which is thread-safe and can be shared across sessions.
// It should be kept alive until all sessions using it are destroyed.
class Environment : public base::subtle::RefCountedThreadSafeBase {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  static base::expected<scoped_refptr<Environment>, std::string> GetInstance(
      const gpu::GPUInfo& gpu_info,
      const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
          ep_package_info_map);

  Environment(base::PassKey<Environment> pass_key, ScopedOrtEnv env);
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  void AddRef() const;
  void Release() const;

  // Returns a vector of selected execution provider devices for WebNN based on
  // the specified device type.
  // In this method, the input `available_devices` are first reordered using
  // WebNN's custom sorting logic. Repeated calls with the same device set and
  // the specified device type will return the same ordered devices, regardless
  // of the input order of `available_devices`. At most 3 EP devices will be
  // selected.
  // TODO(crbug.com/444049496): Log these selected EP devices when ORT logging
  // level is set to VERBOSE or INFO.
  static std::vector<const OrtEpDevice*> SelectEpDevicesForDeviceType(
      base::span<const OrtEpDevice* const> available_devices,
      mojom::Device device_type);

  // Returns a span of registered execution provider devices in `env`. The span
  // is guaranteed to be valid until `env_` is released or the list of execution
  // providers is modified.
  //
  // Thread safety note:
  // The provider list is only modified during Environment initialization and is
  // immutable for the lifetime of the Environment object. Therefore, it is safe
  // for multiple threads to hold and use the returned span concurrently.
  base::span<const OrtEpDevice* const> GetRegisteredEpDevices() const;

  // Get combined EP workarounds for the EPs that will be selected according to
  // the given device type.
  EpWorkarounds GetEpWorkarounds(mojom::Device device_type) const;

  const OrtEnv* get() const { return env_.get(); }

  // Get all EP-specific session configuration entries for the EPs that will be
  // selected according to the given device type.
  std::vector<SessionConfigEntry> GetEpConfigEntries(
      mojom::Device device_type) const;

 private:
  static base::expected<scoped_refptr<Environment>, std::string> Create(
      const gpu::GPUInfo& gpu_info,
      const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
          ep_package_info_map);

  ~Environment();

  ScopedOrtEnv env_;

  static base::Lock& GetLock();
  // Make `Environment` a singleton to avoid duplicate `OrtEnv` creation.
  static raw_ptr<Environment> instance_ GUARDED_BY(GetLock());
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ENVIRONMENT_H_
