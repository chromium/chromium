// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_COMPILER_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_COMPILER_CONTEXT_IMPL_ORT_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/graph_builder_context.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"

namespace webnn {

class WebNNTensorImpl;
class WebNNConstantOperand;

namespace ort {

class Environment;
class SessionOptions;

// Manages graph compilation. Compiled results are sent back to the GPU process
// for inference. Runs in the WebNN Compiler utility process.
class CompilerContextImplOrt final : public GraphBuilderContext,
                                     public mojom::WebNNCompilerContext {
 public:
  static std::unique_ptr<CompilerContextImplOrt> Create(
      base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info,
      mojom::CreateContextOptionsPtr options,
      ContextProperties properties,
      mojo::PendingRemote<mojom::WebNNModelLoader> model_loader);

  CompilerContextImplOrt(
      scoped_refptr<Environment> env,
      mojom::CreateContextOptionsPtr options,
      ContextProperties properties,
      mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
      base::PassKey<CompilerContextImplOrt> pass_key);

  CompilerContextImplOrt(const CompilerContextImplOrt&) = delete;
  CompilerContextImplOrt& operator=(const CompilerContextImplOrt&) = delete;

  ~CompilerContextImplOrt() override;

  // mojom::WebNNCompilerContext:
  void CreateGraphBuilder(
      mojo::PendingReceiver<mojom::WebNNGraphBuilder> receiver) override;

  // GraphBuilderContext:
  const ContextProperties& properties() const override;
  const mojom::CreateContextOptions& options() const override;
  void BuildGraph(
      mojo::PendingReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
          constant_tensor_operands,
      BuildGraphCallback callback) override;

 private:
  struct CompilationResult;

  // Runs on the graph compilation task runner to compile the graph.
  static base::expected<std::unique_ptr<CompilationResult>, mojom::ErrorPtr>
  CompileOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      scoped_refptr<SessionOptions> session_options,
      scoped_refptr<Environment> env,
      ContextProperties context_properties,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands);

  // Called on the main thread after compilation completes.
  void DidCompile(mojo::PendingReceiver<mojom::WebNNGraph> graph_receiver,
                  WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
                  BuildGraphCallback callback,
                  base::expected<std::unique_ptr<CompilationResult>,
                                 mojom::ErrorPtr> result);

  // Called when the model loader mojo pipe disconnects.
  void OnModelLoaderDisconnected();

  ContextProperties properties_;
  mojom::CreateContextOptionsPtr options_;

  // Reverse channel to GPU process for sending compiled graphs.
  mojo::Remote<mojom::WebNNModelLoader> model_loader_;

  // ORT environment and session options for compilation.
  scoped_refptr<Environment> env_;
  scoped_refptr<SessionOptions> session_options_;

  // Cancels pending compilation tasks when destructed.
  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_COMPILER_CONTEXT_IMPL_ORT_H_
