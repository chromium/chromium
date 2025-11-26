// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/device_allocator.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

// `ContextImplOrt` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplOrt` which uses ONNX Runtime for inference.
class ContextImplOrt final : public WebNNContextImpl {
 public:
  // Constructs a new `ContextImplOrt`. Must be called on `owning_task_runner`.
  static std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> Create(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      const EpWorkarounds& ep_workarounds,
      mojom::CreateContextOptionsPtr options,
      mojom::Device device_type,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      scoped_refptr<Environment> env,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedSequence> sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      ScopedTrace scoped_trace);

  ContextImplOrt(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 base::WeakPtr<WebNNContextProviderImpl> context_provider,
                 const EpWorkarounds& ep_workarounds,
                 mojom::CreateContextOptionsPtr options,
                 mojom::Device device_type,
                 mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
                 mojo::ScopedDataPipeProducerHandle read_tensor_producer,
                 scoped_refptr<Environment> env,
                 gpu::CommandBufferId command_buffer_id,
                 std::unique_ptr<ScopedSequence> sequence,
                 scoped_refptr<gpu::MemoryTracker> memory_tracker,
                 scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
                 gpu::SharedImageManager* shared_image_manager,
                 scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ContextImplOrt(const WebNNContextImpl&) = delete;
  ContextImplOrt& operator=(const ContextImplOrt&) = delete;


  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  static ContextProperties GetContextProperties(bool resample2d_limit_to_nchw);

  scoped_refptr<Environment> env() const { return env_; }

  scoped_refptr<SessionOptions> session_options() const {
    return session_options_;
  }

 private:
  ~ContextImplOrt() override;

  void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      CreateGraphImplCallback callback) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   mojom::TensorInfoPtr tensor_info) override;

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorFromSharedImageImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNTensorImpl::RepresentationPtr representation) override;

  scoped_refptr<Environment> env_;

  // The session options are shared among all the sessions created by this
  // context.
  scoped_refptr<SessionOptions> session_options_;

  // The device allocator used for device tensor creation. May be nullptr if
  // device tensor is not supported.
  scoped_refptr<DeviceAllocator> device_allocator_;

  base::WeakPtrFactory<ContextImplOrt> weak_factory_{this};
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
