// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_DISPATCH_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_DISPATCH_CONTEXT_IMPL_ORT_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"

namespace webnn::ort {

// GPU-process context for dispatch-only ORT operations when the Compiler
// process handles graph building and compilation. Inherits tensor management
// and session infrastructure from ContextImplOrt, and adds the ability to
// receive compiled models from the Compiler process via WebNNModelLoader.
//
// Created by WebNNContextProviderImpl when kWebNNCompilerProcess is enabled.
class COMPONENT_EXPORT(WEBNN_SERVICE) DispatchContextImplOrt final
    : public ContextImplOrt,
      public mojom::WebNNModelLoader {
 public:
  // Same factory signature as ContextImplOrt::Create, but returns a
  // DispatchContextImplOrt.
  static std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> Create(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      mojom::CreateContextOptionsPtr options,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      scoped_refptr<Environment> env,
      std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      ScopedTrace scoped_trace);

  DispatchContextImplOrt(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      const EpWorkarounds& ep_workarounds,
      bool dequantize_linear_input_support_int32,
      mojom::CreateContextOptionsPtr options,
      scoped_refptr<SessionOptions> session_options,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      scoped_refptr<Environment> env,
      std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  DispatchContextImplOrt(const DispatchContextImplOrt&) = delete;
  DispatchContextImplOrt& operator=(const DispatchContextImplOrt&) = delete;

  // Binds the model loader receiver, typically called after Browser wires
  // the Compiler process pipes. The model loader receiver is the GPU end
  // of the Compiler→GPU reverse channel.
  void BindModelLoader(mojo::PendingReceiver<mojom::WebNNModelLoader> receiver);

  base::WeakPtr<DispatchContextImplOrt> GetWeakPtr();

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

 private:
  ~DispatchContextImplOrt() override;

  // mojom::WebNNModelLoader:
  void LoadCompiledGraph(
      mojom::CompiledGraphPtr compiled_graph,
      mojo::PendingReceiver<mojom::WebNNGraph> graph_receiver,
      LoadCompiledGraphCallback callback) override;

  mojo::Receiver<mojom::WebNNModelLoader> model_loader_receiver_{this};

  base::WeakPtrFactory<DispatchContextImplOrt> weak_factory_{this};
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_DISPATCH_CONTEXT_IMPL_ORT_H_
