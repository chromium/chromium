// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_
#define SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_

#include <wrl.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/Microsoft.Windows.AI.MachineLearning.h"

namespace webnn {

class ExecutionProviderInitializer {
 public:
  // A flat map with EP name as the key and package info as the value.
  using EpPackageInfoMap = base::flat_map<std::string, mojom::EpPackageInfoPtr>;

  ~ExecutionProviderInitializer() = delete;
  ExecutionProviderInitializer(const ExecutionProviderInitializer&) = delete;
  ExecutionProviderInitializer& operator=(const ExecutionProviderInitializer&) =
      delete;

  static ExecutionProviderInitializer* GetInstance();

  // Try to ensure the EPs are ready and retrieve the package info of all
  // available EPs. If initialization is incomplete, the callback is queued and
  // invoked upon completion. If already initialized, the callback is invoked
  // immediately with cached package info.
  void EnsureExecutionProvidersReady(
      base::OnceCallback<void(EpPackageInfoMap)> callback);

 private:
  friend class base::NoDestructor<ExecutionProviderInitializer>;

  ExecutionProviderInitializer();

  // Try to initialize the EPs, will trigger the installation of the required EP
  // packages.
  void Initialize(
      std::vector<Microsoft::WRL::ComPtr<
          ::ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider>>
          providers);
  // Called when the installed execution providers are ensured ready. Invokes
  // all the pending callbacks with cached EP package info.
  void OnInitialize();

  void AddExecutionProviderPackageInfo(
      std::pair<std::string, mojom::EpPackageInfoPtr> ep_package_info);

  // Cached package info of EPs that are ready for use.
  EpPackageInfoMap ep_package_info_map_;

  // Pending callbacks to be invoked once initialization is complete.
  base::queue<base::OnceCallback<void(EpPackageInfoMap)>> pending_callbacks_;

  bool initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExecutionProviderInitializer> weak_factory_{this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_
