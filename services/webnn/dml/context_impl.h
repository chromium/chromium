// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_DML_CONTEXT_IMPL_H_

#include <d3d12.h>

#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_feature_info.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn::dml {

class Adapter;
class CommandRecorder;

// `ContextImpl` is created by `WebNNContextProviderImpl` and responsible for
// creating `GraphImpl` and `BufferImpl` of DirectML backend for Windows
// platform. The `Adapter` instance is shared by all `GraphImpl` and
// `BufferImpl` created by this context.
class ContextImpl final : public WebNNContextImpl {
 public:
  ContextImpl(scoped_refptr<Adapter> adapter,
              mojo::PendingReceiver<mojom::WebNNContext> receiver,
              WebNNContextProviderImpl* context_provider,
              std::unique_ptr<CommandRecorder> command_recorder,
              const gpu::GpuFeatureInfo& gpu_feature_info);

  ContextImpl(const WebNNContextImpl&) = delete;
  ContextImpl& operator=(const ContextImpl&) = delete;

  ~ContextImpl() override;

  void ReadBuffer(const WebNNBufferImpl& src_buffer,
                  mojom::WebNNBuffer::ReadBufferCallback callback);

  void WriteBuffer(const WebNNBufferImpl& dst_buffer,
                   mojo_base::BigBuffer src_buffer);

 private:
  void CreateGraphImpl(mojom::GraphInfoPtr graph_info,
                       CreateGraphCallback callback) override;

  std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  // Begins recording commands needed for context operations.
  // If recording failed, calling this function will recreate the recorder to
  // allow recording to start again.
  HRESULT StartRecordingIfNecessary();

  // Report and release the the command recorder on error when it
  // couldn't be closed normally by CommandRecorder::CloseAndExecute.
  void HandleRecordingError(std::string_view error_message, HRESULT hr);

  // After the download is completed, copy the data from the GPU readback
  // buffer into a BigBuffer then run the callback to send it to the render
  // process.
  void OnReadbackComplete(
      Microsoft::WRL::ComPtr<ID3D12Resource> download_buffer,
      size_t read_byte_size,
      mojom::WebNNBuffer::ReadBufferCallback callback,
      HRESULT hr);

  // After the upload completes, tell the queue to immediately
  // release the staging buffer used for the GPU upload.
  void OnUploadComplete(HRESULT hr);

  // The `Adapter` instance shared by all `GraphImpl` created by this context.
  scoped_refptr<Adapter> adapter_;

  // The `CommandRecorder` instance used exclusively by this context.
  std::unique_ptr<CommandRecorder> command_recorder_;

  const raw_ref<const gpu::GpuFeatureInfo> gpu_feature_info_;

  base::WeakPtrFactory<ContextImpl> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
