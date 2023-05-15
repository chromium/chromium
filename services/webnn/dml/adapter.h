// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_ADAPTER_H_
#define SERVICES_WEBNN_DML_ADAPTER_H_

#include <DirectML.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

#include "base/memory/ref_counted.h"
#include "services/webnn/dml/command_queue.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class Adapter final : public base::RefCounted<Adapter> {
 public:
  static scoped_refptr<Adapter> Create(ComPtr<IDXGIAdapter> dxgi_adapter);

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

  IDXGIAdapter* dxgi_adapter() const { return dxgi_adapter_.Get(); }

  ID3D12Device* d3d12_device() const { return d3d12_device_.Get(); }

  IDMLDevice* dml_device() const { return dml_device_.Get(); }

  CommandQueue* command_queue() const { return command_queue_.get(); }

 private:
  friend class base::RefCounted<Adapter>;
  Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
          ComPtr<ID3D12Device> d3d12_device,
          ComPtr<IDMLDevice> dml_device,
          std::unique_ptr<CommandQueue> command_queue);
  ~Adapter();

  ComPtr<IDXGIAdapter> dxgi_adapter_;
  ComPtr<ID3D12Device> d3d12_device_;
  ComPtr<IDMLDevice> dml_device_;
  std::unique_ptr<CommandQueue> command_queue_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_ADAPTER_H_
