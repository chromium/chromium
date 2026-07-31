// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_COMPILER_CONTEXT_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_COMPILER_CONTEXT_IMPL_COREML_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/webnn/graph_builder_context.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"

namespace webnn::coreml {

// Manages CoreML graph compilation. Compiled results (.mlmodelc directory) are
// sent back to the GPU process for inference via Browser-brokered copy. Runs
// in the WebNN Compiler utility process.
class COMPONENT_EXPORT(WEBNN_SERVICE) API_AVAILABLE(macos(14.4))
    CompilerContextImplCoreml final : public GraphBuilderContext,
                                      public mojom::WebNNCompilerContext {
 public:
  static std::unique_ptr<CompilerContextImplCoreml> Create(
      mojom::CreateContextOptionsPtr options,
      ContextProperties properties,
      mojo::PendingRemote<mojom::WebNNModelLoader> model_loader);

  CompilerContextImplCoreml(
      mojom::CreateContextOptionsPtr options,
      ContextProperties properties,
      mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
      base::PassKey<CompilerContextImplCoreml> pass_key);

  CompilerContextImplCoreml(const CompilerContextImplCoreml&) = delete;
  CompilerContextImplCoreml& operator=(const CompilerContextImplCoreml&) =
      delete;

  ~CompilerContextImplCoreml() override;

  // mojom::WebNNCompilerContext:
  void CreateGraphBuilder(
      mojo::PendingReceiver<mojom::WebNNGraphBuilder> receiver) override;

  // GraphBuilderContext:
  const ContextProperties& properties() const override;
  const mojom::CreateContextOptions& options() const override;
  void BuildGraph(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      BuildGraphCallback callback) override;

 private:
  struct CompilationResult {
    CompilationResult(
        base::ScopedTempDir compiled_model_dir,
        base::flat_map<std::string, std::string> input_name_to_coreml_name,
        base::flat_map<std::string, std::string> output_name_to_coreml_name);
    ~CompilationResult();
    base::ScopedTempDir compiled_model_dir;
    base::flat_map<std::string, std::string> input_name_to_coreml_name;
    base::flat_map<std::string, std::string> output_name_to_coreml_name;
  };

  using CompileCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<CompilationResult>, mojom::ErrorPtr>)>;

  static void CompileOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      ContextProperties context_properties,
      mojom::Device device,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CompileCallback callback);

  void DidCompile(BuildGraphCallback callback,
                  base::expected<std::unique_ptr<CompilationResult>,
                                 mojom::ErrorPtr> result);

  ContextProperties properties_;
  mojom::CreateContextOptionsPtr options_;

  mojo::Remote<mojom::WebNNModelLoader> model_loader_;

  base::WeakPtrFactory<CompilerContextImplCoreml> weak_ptr_factory_{this};
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_COMPILER_CONTEXT_IMPL_COREML_H_
