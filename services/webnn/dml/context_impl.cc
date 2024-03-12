// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/context_impl.h"

#include <limits>

#include "base/bits.h"
#include "base/check.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/buffer_impl.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"

namespace webnn::dml {

ContextImpl::ContextImpl(scoped_refptr<Adapter> adapter,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider,
                         std::unique_ptr<CommandRecorder> command_recorder,
                         const gpu::GpuFeatureInfo& gpu_feature_info)
    : WebNNContextImpl(std::move(receiver), context_provider),
      adapter_(std::move(adapter)),
      command_recorder_(std::move(command_recorder)),
      gpu_feature_info_(gpu_feature_info) {
  CHECK(command_recorder_);
}

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  GraphImpl::CreateAndBuild(adapter_->command_queue(), adapter_->dml_device(),
                            std::move(graph_info), std::move(callback),
                            gpu_feature_info_->IsWorkaroundEnabled(
                                gpu::DML_EXECUTION_DISABLE_META_COMMANDS));
}

std::unique_ptr<WebNNBufferImpl> ContextImpl::CreateBufferImpl(
    mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // DML requires resources to be in multiple of 4 bytes.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
  constexpr uint64_t kDMLBufferAlignment = 4ull;
  if (std::numeric_limits<uint64_t>::max() - kDMLBufferAlignment <
      buffer_info->size) {
    DLOG(ERROR) << "Buffer is too large to create.";
    return nullptr;
  }

  const uint64_t aligned_buffer_byte_size =
      base::bits::AlignUp(buffer_info->size, kDMLBufferAlignment);

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
  HRESULT hr = command_recorder_->CreateDefaultBuffer(
      aligned_buffer_byte_size, L"WebNN_Default_Buffer_External", buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create the default buffer: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  // The receiver bound to WebNNBufferImpl.
  //
  // Safe to use ContextImpl* because this context owns the buffer
  // being connected and that context cannot destruct before the buffer.
  return std::make_unique<BufferImpl>(std::move(receiver), std::move(buffer),
                                      this, buffer_info->size, buffer_handle);
}

}  // namespace webnn::dml
