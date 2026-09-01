// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ENVIRONMENT_H_
#define SERVICES_WEBNN_ORT_ENVIRONMENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace base {
class FilePath;
}

namespace webnn::ort {

// A wrapper of `OrtEnv` which is thread-safe and can be shared across sessions.
// It should be kept alive until all sessions using it are destroyed.
class COMPONENT_EXPORT(WEBNN_SERVICE) Environment
    : public base::subtle::RefCountedThreadSafeBase {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // Returns the singleton instance of `Environment` if it has been created, or
  // null if it has not been created yet. This is used by WebNN Internals
  // to check if the ORT environment has been initialized without triggering its
  // initialization.
  static std::optional<scoped_refptr<Environment>> GetInstance();

  // Creates an `Environment` instance if it is not created yet and returns a
  // reference-counted pointer to it. The returned `Environment` instance will
  // be shared by all sessions in WebNN.
  static base::expected<scoped_refptr<Environment>, std::string>
  GetOrCreateInstance(
      const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
          ep_package_info_map);

  // Creates an `Environment` instance for the Compiler process, which only
  // registers the EP for the given `target_device`. This also compiles a
  // trivial graph to warm up the target EP device which ensures the libraries
  // required for offline compilation are loaded. The caller is responsible for
  // holding the returned instance for the lifetime of the Compiler process to
  // prevent the environment from being destroyed.
  //
  // Must be called only once before sandbox lockdown.
  static base::expected<scoped_refptr<Environment>, std::string>
  InitializeForCompilerProcess(const base::FilePath& ep_library_path,
                               const EpDeviceInfo& target_device);

  Environment(base::PassKey<Environment> pass_key, ScopedOrtEnv env);
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  void AddRef() const;
  void Release() const;

  // Returns a vector of selected execution provider devices for WebNN. First
  // try to select only one EP device specified by `kWebNNOrtEpDevice` user
  // switch. If no user switch is specified, select EP devices based on the
  // given device type.
  // If select based on the given device type, the input `available_devices` are
  // first reordered using WebNN's custom sorting logic. Repeated calls with the
  // same device set and the specified device type will return the same ordered
  // devices, regardless of the input order of `available_devices`. At most 3 EP
  // devices will be selected.
  static std::vector<const OrtEpDevice*> SelectEpDevices(
      base::span<const OrtEpDevice* const> available_devices,
      OrtHardwareDeviceType device_type);

  // Selects the first registered EP device matching `device_type` for use by
  // the Compiler process. Returns nullopt if no matching device is found.
  std::optional<EpDeviceInfo> SelectEpDeviceForCompiler(
      OrtHardwareDeviceType device_type);

  // Returns true if the execution provider name of `device` matches any of the
  // names in `ep_names`.
  static bool IsEpDevice(const OrtEpDevice* device,
                         base::span<const std::string_view> ep_names);

  // Returns a span of registered execution provider devices in `env`. The span
  // is guaranteed to be valid until `env_` is released or the list of execution
  // providers is modified.
  //
  // Thread safety note:
  // The provider list is only modified during Environment initialization and is
  // immutable for the lifetime of the Environment object. Therefore, it is safe
  // for multiple threads to hold and use the returned span concurrently.
  base::span<const OrtEpDevice* const> GetRegisteredEpDevices() const;

  // Returns the registered execution provider device matching `device_info`, or
  // nullptr if no matching device is found. The returned pointer is guaranteed
  // to be valid until `env_` is released or the list of execution providers is
  // modified.
  //
  // Thread safety note:
  // The provider list is only modified during Environment initialization and is
  // immutable for the lifetime of the Environment object. Therefore, it is safe
  // for multiple threads to hold and use the returned pointer concurrently.
  const OrtEpDevice* FindRegisteredEpDevice(
      const EpDeviceInfo& device_info) const;

  // Returns a vector of execution provider details for all registered EPs in
  // this environment. This is used for introspection purposes in WebNN
  // Internals.
  std::vector<mojom::WebNNExecutionProviderDetailsPtr> GetAvailableEpDetails()
      const;

  // Returns a vector of execution provider details for all selected EPs for a
  // given device type. This is used for introspection purposes in WebNN
  // Internals.
  std::vector<mojom::WebNNExecutionProviderDetailsPtr> GetSelectedEpDetails(
      OrtHardwareDeviceType device_type) const;

  // Get combined EP workarounds for the EPs that will be selected according to
  // the given device type.
  EpWorkarounds GetEpWorkarounds(OrtHardwareDeviceType device_type) const;

  const OrtEnv* get() const { return env_.get(); }

  scoped_refptr<base::SequencedTaskRunner> graph_compilation_task_runner()
      const {
    return graph_compilation_task_runner_;
  }

 private:
  static base::expected<scoped_refptr<Environment>, std::string> Create(
      const base::flat_map<std::string, mojom::EpPackageInfoPtr>&
          ep_package_info_map) EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  static base::expected<scoped_refptr<Environment>, std::string>
  CreateForCompilerProcess(const base::FilePath& ep_library_path,
                           const EpDeviceInfo& target_device);

  // Compiles a trivial graph on the target device to ensure the required
  // libraries are loaded for the Compiler process. Fails if the EP cannot
  // compile in this process, which is not necessarily a defect: an EP may
  // depend on libraries that cannot be loaded under the compiler process's
  // mitigations.
  base::expected<void, std::string> WarmupEpDeviceForCompilerProcess(
      const EpDeviceInfo& target_device);

  ~Environment();

  ScopedOrtEnv env_;

  // A sequence runner for graph compilation tasks.
  const scoped_refptr<base::SequencedTaskRunner> graph_compilation_task_runner_;

  static base::Lock& GetLock();
  // Make `Environment` a singleton to avoid duplicate `OrtEnv` creation.
  // A plain pointer is used rather than `raw_ptr` so that this static does not
  // require a dynamic initializer or an exit-time destructor. `instance_` is
  // cleared in `Release()` before the object is deleted, so it never dangles.
  static Environment* instance_ GUARDED_BY(GetLock());
  // Returns the set of dependent EP package family names to prevent repeated
  // calls to `AddPackageDependency` for EP packages in the GPU process
  // whenever an `Environment` is created. This set is only accessed in
  // `Environment::Create()` that is already protected by `GetLock()`.
  static base::flat_set<std::wstring>& GetDependentEpPackages()
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ENVIRONMENT_H_
