// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_ADAPTER_H_
#define SERVICES_WEBNN_DML_ADAPTER_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "third_party/microsoft_dxheaders/include/directml.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

// Windows SDK headers should be included after DirectX headers.
#include <dxgi.h>
#include <wrl.h>

namespace webnn::dml {

class CommandQueue;

// Adapters represent physical devices and are responsible for device discovery.
// An `Adapter` instance creates and maintains corresponding `IDXGIAdapter` or
// `IDXCoreAdapter`, `ID3D12Device`, `IDMLDevice1` and
// `webnn::dml::CommandQueue` for a physical adapter. A single `Adapter`
// instance is shared and reference-counted by all `GraphImplDml` of the same
// adapter. The `Adapter` instance is created upon the first `GraphImplDml` call
// `Adapter::GetGpuInstance()` or `Adapter::GetNpuInstance()` and is released
// when the last `GraphImplDml` is destroyed.
class COMPONENT_EXPORT(WEBNN_SERVICE) Adapter final
    : public base::RefCountedThreadSafe<Adapter> {
 public:
  // Get the shared `Adapter` instance. If `Adapter` instance already exists,
  // that one is returned regardless of whether the `dxgi_adapter` matches.
  // TODO(crbug.com/40277628): Support `Adapter` instance for other adapters.
  //
  // This method is not thread-safe and should only be called on the GPU main
  // thread.
  //
  // The returned `Adapter` is guaranteed to support a feature level equal to
  // or greater than DML_FEATURE_LEVEL_4_0.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> GetGpuInstance(
      Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter);

  // Similar to `GetGpuInstance()` but use the first enumerated DXGI adapter.
  // The returned `Adapter` is guaranteed to support a feature level equal to or
  // greater than DML_FEATURE_LEVEL_2_0 because that is where DMLCreateDevice1
  // was introduced.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr>
  GetGpuInstanceForTesting();

  // Similar to the `GetGpuInstance` method above, get the shared `Adapter`
  // instance for NPU.  The returned `Adapter` is guaranteed to support a
  // feature level equal to or greater than DML_FEATURE_LEVEL_6_4.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> GetNpuInstance(
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::GPUInfo& gpu_info);

  // Similar to the `GetNpuInstance` method above, get the shared NPU `Adapter`
  // instance for testing.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr>
  GetNpuInstanceForTesting();

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  ID3D12Device* d3d12_device() const { return d3d12_device_.Get(); }

  IDMLDevice1* dml_device() const { return dml_device_.Get(); }

  CommandQueue* command_queue() const { return command_queue_.get(); }

  DML_FEATURE_LEVEL max_supported_feature_level() const {
    return max_supported_dml_feature_level_;
  }

  CommandQueue* init_command_queue_for_npu() const {
    return init_command_queue_for_npu_.get();
  }

  base::SequencedTaskRunner* init_task_runner_for_npu() const {
    return init_task_runner_for_npu_.get();
  }

  bool IsNPU() const { return npu_instance_ == this; }

  // Enable the debug layer (requires the Graphics Tools "optional feature").
  // Must be called prior to Adapter::GetGpuInstance() since the D3D12 device
  // must be created after the debug layer is enabled.
  // TODO(crbug.com/40206287): move this once adapter enumeration is
  // implemented.
  static void EnableDebugLayerForTesting();

  bool IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL feature_level) const;

  // Determines if IDMLDevice1::CompileGraph can be used.
  bool IsDMLDeviceCompileGraphSupportedForTesting() const;

  // Indicates whether the underlying D3D12 device supports UMA (Unified Memory
  // Architecture).
  bool IsUMA() const { return is_uma_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, GetGpuInstance);
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, GetNpuInstance);

  friend class base::RefCountedThreadSafe<Adapter>;
  Adapter(Microsoft::WRL::ComPtr<IUnknown> dxgi_or_dxcore_adapter,
          Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device,
          Microsoft::WRL::ComPtr<IDMLDevice1> dml_device,
          scoped_refptr<CommandQueue> command_queue,
          scoped_refptr<CommandQueue> init_command_queue_for_npu,
          DML_FEATURE_LEVEL max_supported_dml_feature_level,
          bool is_uma);
  ~Adapter();

  // `dxgi_or_dxcore_adapter` must be either `IDXGIAdapter` or `IDXCoreAdapter'.
  // DXGI is older and more broadly supported, capable of enumerating GPUs,
  // while NPUs are only able to be enumerated by DXCore.
  // The `min_required_dml_feature_level` parameter allows us to run a portion
  // of the test suite on machines that have feature levels less than the one
  // that will be required for full WebNN.
  // TODO(issues.chromium.org/331369802): Remove min/max DML feature level
  // parameters for `dml::Adapter` creation in production code.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> Create(
      Microsoft::WRL::ComPtr<IUnknown> dxgi_or_dxcore_adapter,
      DML_FEATURE_LEVEL min_required_dml_feature_level);

  Microsoft::WRL::ComPtr<IUnknown> dxgi_or_dxcore_adapter_;

  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;
  Microsoft::WRL::ComPtr<IDMLDevice1> dml_device_;
  scoped_refptr<CommandQueue> command_queue_;
  // It's dedicated to initialize graph on background thread for the NPU
  // adapter, it's nullptr for the GPU adapter.
  scoped_refptr<CommandQueue> init_command_queue_for_npu_;
  // It's used to post the graph initialization tasks to the background thread
  // and guarantee the tasks are executed in order for the NPU adapter, it's
  // nullptr for the GPU adapter.
  scoped_refptr<base::SequencedTaskRunner> init_task_runner_for_npu_;

  DML_FEATURE_LEVEL max_supported_dml_feature_level_ = DML_FEATURE_LEVEL_1_0;

  static bool enable_d3d12_debug_layer_for_testing_;

  // Store the info of D3D12_FEATURE_DATA_ARCHITECTURE.
  const bool is_uma_ = false;

  static Adapter* gpu_instance_;  // Static instance for dxgi adapter.
  static Adapter* npu_instance_;  // Static instance for dxcore adapter.
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_ADAPTER_H_
