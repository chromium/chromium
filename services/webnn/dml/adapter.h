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
  // TODO(crbug.com/1273291): Support `Adapter` instance for other adapters.
  static scoped_refptr<Adapter> GetInstance();

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  IDXGIAdapter* dxgi_adapter() const { return dxgi_adapter_.Get(); }

  ID3D12Device* d3d12_device() const { return d3d12_device_.Get(); }

  IDMLDevice* dml_device() const { return dml_device_.Get(); }

  CommandQueue* command_queue() const { return command_queue_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, CreateAdapterFromAngle);
  FRIEND_TEST_ALL_PREFIXES(WebNNAdapterTest, GetInstance);

  friend class base::RefCounted<Adapter>;
  Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
          ComPtr<ID3D12Device> d3d12_device,
          ComPtr<IDMLDevice> dml_device,
          scoped_refptr<CommandQueue> command_queue);
  ~Adapter();

  static scoped_refptr<Adapter> Create(ComPtr<IDXGIAdapter> dxgi_adapter);

  ComPtr<IDXGIAdapter> dxgi_adapter_;
  ComPtr<ID3D12Device> d3d12_device_;
  ComPtr<IDMLDevice> dml_device_;
  scoped_refptr<CommandQueue> command_queue_;

  static Adapter* instance_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_ADAPTER_H_
