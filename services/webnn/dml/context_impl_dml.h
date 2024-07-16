// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_CONTEXT_IMPL_DML_H_
#define SERVICES_WEBNN_DML_CONTEXT_IMPL_DML_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_feature_info.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

namespace webnn::dml {

class Adapter;
class BufferImplDml;
class CommandRecorder;

// `ContextImplDml` is created by `WebNNContextProviderImpl` and responsible for
// creating `GraphImplDml` and `BufferImplDml` of DirectML backend for Windows
// platform. The `Adapter` instance is shared by all `GraphImplDml` and
// `BufferImplDml` created by this context.
class ContextImplDml final : public WebNNContextImpl {
 public:
  ContextImplDml(scoped_refptr<Adapter> adapter,
                 mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 mojo::PendingRemote<mojom::WebNNContextClient> client_remote,
                 WebNNContextProviderImpl* context_provider,
                 mojom::CreateContextOptionsPtr options,
                 std::unique_ptr<CommandRecorder> command_recorder,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 base::UnguessableToken context_handle);

  ContextImplDml(const WebNNContextImpl&) = delete;
  ContextImplDml& operator=(const ContextImplDml&) = delete;

  ~ContextImplDml() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  void ReadBuffer(BufferImplDml* src_buffer,
                  mojom::WebNNBuffer::ReadBufferCallback callback);

  void WriteBuffer(BufferImplDml* dst_buffer, mojo_base::BigBuffer src_buffer);

  // Some errors like `E_OUTOFMEMORY`, `DXGI_ERROR_DEVICE_REMOVED` and
  // `DXGI_ERROR_DEVICE_RESET` are treated as `context lost` errors, other
  // errors will crash the GPU process.
  //
  // TODO(crbug.com/349640008): For the `context lost` errors, we should
  // gracefully terminate the GPU process.
  void HandleContextLostOrCrash(std::string_view message_for_log, HRESULT hr);

 private:
  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      CreateGraphImplCallback callback) override;

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

  // The `Adapter` instance shared by all `GraphImplDml` created by this
  // context.
  scoped_refptr<Adapter> adapter_;

  // The `CommandRecorder` instance used exclusively by this context.
  std::unique_ptr<CommandRecorder> command_recorder_;

  const raw_ref<const gpu::GpuFeatureInfo> gpu_feature_info_;

  base::WeakPtrFactory<ContextImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_DML_H_
