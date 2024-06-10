// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/context_impl_dml.h"

#include <limits>

#include "base/bits.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/buffer_impl_dml.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/graph_impl_dml.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"

namespace webnn::dml {

namespace {

using Microsoft::WRL::ComPtr;

mojom::ContextPropertiesPtr GetProperties() {
  return mojom::ContextProperties::New(
      /*conv2d_input_layout=*/mojom::InputOperandLayout::kChannelsFirst);
}

}  // namespace

ContextImplDml::ContextImplDml(
    scoped_refptr<Adapter> adapter,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    std::unique_ptr<CommandRecorder> command_recorder,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : WebNNContextImpl(std::move(receiver), context_provider, GetProperties()),
      adapter_(std::move(adapter)),
      command_recorder_(std::move(command_recorder)),
      gpu_feature_info_(gpu_feature_info) {
  CHECK(command_recorder_);
}

ContextImplDml::~ContextImplDml() = default;

void ContextImplDml::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  GraphImplDml::CreateAndBuild(adapter_, weak_factory_.GetWeakPtr(),
                            std::move(graph_info), std::move(callback),
                            gpu_feature_info_->IsWorkaroundEnabled(
                                gpu::DML_EXECUTION_DISABLE_META_COMMANDS));
}

std::unique_ptr<WebNNBufferImpl> ContextImplDml::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
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

  HRESULT hr = S_OK;

  // If adapter supports UMA, create the custom heap with CPU memory pool. The
  // CPU will directly read/write to this heap if the GPU isn't using it.
  ComPtr<ID3D12Resource> buffer;
  if (adapter_->IsUMA()) {
    // TODO(crbug.com/40278771): consider introducing buffer usages for INPUT or
    // OUTPUT since using upload-equivelent custom heaps everywhere could be
    // inefficient.
    hr = CreateCustomUploadBuffer(
        adapter_->d3d12_device(), aligned_buffer_byte_size,
        L"WebNN_Custom_Upload_Buffer_External", buffer);
  } else {
    // Create a default buffer that can be accessed only by GPU.
    // The CPU must use a staging buffer to read/write to this buffer.
    hr = CreateDefaultBuffer(adapter_->d3d12_device(), aligned_buffer_byte_size,
                             L"WebNN_Default_Buffer_External", buffer);
  }

  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to create the external buffer: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  // The receiver bound to WebNNBufferImpl.
  //
  // Safe to use ContextImplDml* because this context owns the buffer
  // being connected and that context cannot destruct before the buffer.
  return std::make_unique<BufferImplDml>(std::move(receiver), std::move(buffer),
                                         this, buffer_info->size,
                                         buffer_handle);
}

void ContextImplDml::ReadBuffer(
    BufferImplDml* src_buffer,
    mojom::WebNNBuffer::ReadBufferCallback callback) {
  const uint64_t src_buffer_size = src_buffer->size();

  // Read size needs to be cast to size_t.
  base::CheckedNumeric<size_t> checked_src_buffer_size(src_buffer_size);
  if (!checked_src_buffer_size.IsValid()) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  HRESULT hr = S_OK;

  // Map entire buffer to readback the output data.
  if (adapter_->IsUMA() && adapter_->command_queue()->GetCompletedValue() >=
                               src_buffer->last_submission_fence_value()) {
    ContextImplDml::OnReadbackComplete(src_buffer->buffer(),
                                       checked_src_buffer_size.ValueOrDie(),
                                       std::move(callback), hr);
    return;
  }

  // Copy the buffer into a staging buffer to readback the output data.
  ComPtr<ID3D12Resource> download_buffer;
  hr = CreateReadbackBuffer(adapter_->d3d12_device(), src_buffer_size,
                            L"WebNN_Readback_Buffer", download_buffer);
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to create the download buffer: "
               << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    return;
  }

  hr = StartRecordingIfNecessary();
  if (FAILED(hr)) {
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    return;
  }

  ReadbackBufferWithBarrier(command_recorder_.get(), download_buffer,
                            src_buffer, src_buffer_size);

  // Submit copy and schedule GPU wait.
  hr = command_recorder_->CloseAndExecute();
  if (FAILED(hr)) {
    HandleRecordingError("Failed to close and execute the command list.", hr);
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    return;
  }

  // The source and readback buffer is held alive during execution by the
  // recorder by calling `ReadbackBufferWithBarrier()` then
  // CommandRecorder::CloseAndExecute().
  adapter_->command_queue()->WaitAsync(base::BindOnce(
      &ContextImplDml::OnReadbackComplete, weak_factory_.GetWeakPtr(),
      std::move(download_buffer), checked_src_buffer_size.ValueOrDie(),
      std::move(callback)));
}

void ContextImplDml::OnReadbackComplete(
    ComPtr<ID3D12Resource> download_buffer,
    size_t read_byte_size,
    mojom::WebNNBuffer::ReadBufferCallback callback,
    HRESULT hr) {
  // Tell the queue to release the downloaded buffer so it may be finally
  // released at the end of this function.
  adapter_->command_queue()->ReleaseCompletedResources();

  if (FAILED(hr)) {
    HandleRecordingError("Failed to download the buffer.", hr);
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    return;
  }

  CHECK(download_buffer);

  // Copy over data from the download buffer to the destination buffer.
  void* mapped_download_data = nullptr;
  hr = download_buffer->Map(0, nullptr, &mapped_download_data);
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to map the download buffer: "
               << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(ToError<mojom::ReadBufferResult>(
        mojom::Error::Code::kUnknownError, "Failed to read buffer."));
    return;
  }

  mojo_base::BigBuffer dst_buffer(base::make_span(
      static_cast<const uint8_t*>(mapped_download_data), read_byte_size));

  download_buffer->Unmap(0, nullptr);

  std::move(callback).Run(
      mojom::ReadBufferResult::NewBuffer(std::move(dst_buffer)));
}

void ContextImplDml::WriteBuffer(BufferImplDml* dst_buffer,
                                 mojo_base::BigBuffer src_buffer) {
  HRESULT hr = S_OK;
  ComPtr<ID3D12Resource> buffer_to_map = dst_buffer->buffer();

  // Create a staging buffer to upload data into when the existing buffer
  // cannot be updated by the CPU.
  if (!adapter_->IsUMA() || adapter_->command_queue()->GetCompletedValue() <
                                dst_buffer->last_submission_fence_value()) {
    hr = CreateUploadBuffer(adapter_->d3d12_device(), src_buffer.size(),
                            L"WebNN_Upload_Buffer", buffer_to_map);
    if (FAILED(hr)) {
      // TODO(crbug.com/41492165): generate error using context.
      LOG(ERROR) << "[WebNN] Failed to create the upload buffer: "
                 << logging::SystemErrorCodeToString(hr);
      return;
    }
  }

  CHECK(buffer_to_map);

  // Copy over data from the source buffer to the mapped buffer.
  void* mapped_buffer_data = nullptr;
  hr = buffer_to_map->Map(0, nullptr, &mapped_buffer_data);
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to map the buffer: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  CHECK(mapped_buffer_data);

  // SAFETY: `buffer_to_map` was constructed with size `src_buffer.size()`.
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(mapped_buffer_data), src_buffer.size()))
      .copy_from(src_buffer);

  buffer_to_map->Unmap(0, nullptr);

  // Uploads are only required when the mapped buffer was a staging buffer.
  if (dst_buffer->buffer() != buffer_to_map.Get()) {
    if (FAILED(StartRecordingIfNecessary())) {
      return;
    }

    UploadBufferWithBarrier(command_recorder_.get(), dst_buffer,
                            std::move(buffer_to_map), src_buffer.size());

    // TODO(crbug.com/40278771): consider not submitting after every write.
    // CloseAndExecute() only needs to be called once, when the buffer is read
    // by another context operation (ex. input into dispatch). Submitting
    // immediately prevents memory usage from increasing; however, it also
    // incurs more overhead due to a near empty command-list getting executed
    // every time.
    hr = command_recorder_->CloseAndExecute();
    if (FAILED(hr)) {
      HandleRecordingError("Failed to close and execute the command list.", hr);
      return;
    }

    // Since the queue owns the upload buffer, it does not need to be provided
    // to OnUploadComplete() and will be finally released once the wait is
    // satisfied.
    adapter_->command_queue()->WaitAsync(base::BindOnce(
        &ContextImplDml::OnUploadComplete, weak_factory_.GetWeakPtr()));
  }
}

void ContextImplDml::OnUploadComplete(HRESULT hr) {
  // Once the upload is complete, tell the queue to de-queue the dst_buffer and
  // upload buffer which immediately releases it.
  adapter_->command_queue()->ReleaseCompletedResources();

  if (FAILED(hr)) {
    HandleRecordingError("Failed to upload the buffer.", hr);
    return;
  }
}

HRESULT ContextImplDml::StartRecordingIfNecessary() {
  // Recreate the recorder on error since resources recorded but
  // not executed would remain alive until this context gets destroyed and
  // this context would be prevented from recording new commands.
  if (!command_recorder_) {
    command_recorder_ = CommandRecorder::Create(adapter_->command_queue(),
                                                adapter_->dml_device());
    if (!command_recorder_) {
      DLOG(ERROR) << "Failed to create the command recorder.";
      return E_FAIL;
    }
  }

  CHECK(command_recorder_);

  // If the recorder is already recording, no need to re-open.
  HRESULT hr = S_OK;
  if (command_recorder_->IsOpen()) {
    return hr;
  }

  // Open the command recorder for recording the context execution commands.
  hr = command_recorder_->Open();
  if (FAILED(hr)) {
    HandleRecordingError("Failed to open the command recorder.", hr);
    return hr;
  }

  CHECK(command_recorder_->IsOpen());

  return hr;
}

void ContextImplDml::HandleRecordingError(std::string_view error_message,
                                       HRESULT hr) {
  LOG(ERROR) << error_message << " " << logging::SystemErrorCodeToString(hr);
  command_recorder_.reset();
}

}  // namespace webnn::dml
