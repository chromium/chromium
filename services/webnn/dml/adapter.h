// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_ADAPTER_H_
#define SERVICES_WEBNN_DML_ADAPTER_H_

#include <DirectML.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class CommandQueue;

// Adapters represent physical devices and are responsible for device discovery.
// An `Adapter` instance creates and maintains corresponding `IDXGIAdapter`,
// `ID3D12Device`, `IDMLDevice` and `webnn::dml::CommandQueue` for a physical
// adapter. A single `Adapter` instance is shared and reference-counted by all
// `webnn::dml::GraphImpl` of the same adapter. The `Adapter` instance is
// created upon the first `webnn::dml::GraphImpl` call `Adapter::GetInstance()`
// and is released when the last ``webnn::dml::GraphImpl` is destroyed.
class Adapter final : public base::RefCounted<Adapter> {
 public:
  // Get the shared `Adapter` instance for the default adapter. At the current
  // stage, the default adapter is queried from ANGLE. This method is not
  // thread-safe and should only be called on the GPU main thread.
  //
  // The returned `Adapter` is guarenteed to support a feature level equal to
  // or greater than the `min_feature_level_required`. This allows tests to
  // specify a lower feature level than what WebNN requires.
  //
  // TODO(crbug.com/1273291): Support `Adapter` instance for other adapters.
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> GetInstance(
      DML_FEATURE_LEVEL min_feature_level_required);

  // Same as GetInstance() but always uses DML_FEATURE_LEVEL_1_0 for testing
  // purposes. Allows tests to run on a per feature level basis using
  // IsDMLFeatureLevelSupported().
  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr>
  GetInstanceForTesting();

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  IDXGIAdapter* dxgi_adapter() const { return dxgi_adapter_.Get(); }

  ID3D12Device* d3d12_device() const { return d3d12_device_.Get(); }

  IDMLDevice* dml_device() const { return dml_device_.Get(); }

  CommandQueue* command_queue() const { return command_queue_.get(); }

  // Enable the debug layer (requires the Graphics Tools "optional feature").
  // Must be called prior to Adapter::GetInstance() since the D3D12 device must
  // be created after the debug layer is enabled.
  // TODO(crbug.com/1273291): move this once adapter enumeration is implemented.
  static void EnableDebugLayerForTesting();

  bool IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL feature_level) const;

  // Determines if IDMLDevice1::CompileGraph can be used.
  bool IsDMLDeviceCompileGraphSupportedForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, CreateAdapterFromAngle);
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, GetInstance);

  friend class base::RefCounted<Adapter>;
  Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
          ComPtr<ID3D12Device> d3d12_device,
          ComPtr<IDMLDevice> dml_device,
          scoped_refptr<CommandQueue> command_queue,
          DML_FEATURE_LEVEL max_feature_level_supported);
  ~Adapter();

  static base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> Create(
      ComPtr<IDXGIAdapter> dxgi_adapter,
      DML_FEATURE_LEVEL min_feature_level_required);

  ComPtr<IDXGIAdapter> dxgi_adapter_;
  ComPtr<ID3D12Device> d3d12_device_;
  ComPtr<IDMLDevice> dml_device_;
  scoped_refptr<CommandQueue> command_queue_;

  DML_FEATURE_LEVEL max_feature_level_supported_ = DML_FEATURE_LEVEL_1_0;

  static bool is_debug_layer_enabled_;

  static Adapter* instance_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_ADAPTER_H_
