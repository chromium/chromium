// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_GRAPH_IMPL_COREML_H_

#import <CoreML/CoreML.h>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::coreml {

class ContextImplCoreml;

// GraphImplCoreml inherits from WebNNGraphImpl to represent a CoreML graph
// implementation. It is mainly responsible for building and compiling a CoreML
// graph from mojom::GraphInfo via GraphBuilderCoreml, then initializing and
// executing the graph. Mac OS 13.0+ is required for model compilation
// https://developer.apple.com/documentation/coreml/mlmodel/3931182-compilemodel
// Mac OS 14.0+ is required to support WebNN logical binary operators because
// the cast operator does not support casting to uint8 prior to Mac OS 14.0.
// Mac OS 14.4 is required to use MLComputePlan.
// https://developer.apple.com/documentation/coreml/mlcomputeplan-1w21n
// CoreML returns bool tensors for logical operators which need to be cast to
// uint8 tensors to match WebNN expectations.
class API_AVAILABLE(macos(14.4)) GraphImplCoreml final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      ContextImplCoreml* context,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      mojom::CreateContextOptionsPtr context_options,
      ContextProperties context_properties,
      WebNNContextImpl::CreateGraphImplCallback callback);

  struct Params;
  GraphImplCoreml(mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
                  base::WeakPtr<WebNNContextImpl> context,
                  std::unique_ptr<Params> params);

  GraphImplCoreml(const GraphImplCoreml&) = delete;
  GraphImplCoreml& operator=(const GraphImplCoreml&) = delete;

 private:
  ~GraphImplCoreml() override;

  // Additional information about the model input that is required
  // for the CoreML backend.
  struct CoreMLFeatureInfo {
    CoreMLFeatureInfo(MLMultiArrayDataType data_type,
                      NSMutableArray* shape,
                      NSMutableArray* stride,
                      std::string_view coreml_name)
        : data_type(data_type),
          shape(shape),
          stride(stride),
          coreml_name(coreml_name) {}

    MLMultiArrayDataType data_type;
    NSMutableArray* __strong shape;
    NSMutableArray* __strong stride;
    std::string coreml_name;
  };

  // Responsible for cleaning up disk artifacts created by the CoreML model
  // compilation process.
  // This also dumps model files to to `switches::kWebNNCoreMlDumpModel` if
  // provided.
  struct ScopedModelPath {
    explicit ScopedModelPath(base::ScopedTempDir file_dir);
    ~ScopedModelPath();
    ScopedModelPath(ScopedModelPath&& other) = default;

    base::ScopedTempDir file_dir;
  };

  // Compile the CoreML model to a temporary .modelc file.
  static void CreateAndBuildOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      mojom::CreateContextOptionsPtr context_options,
      ContextProperties context_properties,
      base::OnceCallback<void(
          base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback);

  static void LoadCompiledModelOnBackgroundThread(
      base::ElapsedTimer compilation_timer,
      base::ScopedTempDir model_file_dir,
      mojom::CreateContextOptionsPtr context_options,
      std::unique_ptr<Params> params,
      base::OnceCallback<void(
          base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback,
      NSURL* compiled_model_url,
      NSError* error);

  static void ReadComputePlan(
      std::unique_ptr<Params> params,
      base::OnceCallback<void(
          base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback,
      ScopedModelPath temp_dir,
      MLComputePlan* compute_plan,
      NSError* compute_plan_error);

  static void DidCreateAndBuild(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<Params>, mojom::ErrorPtr> result);

  // Execute the compiled platform graph asynchronously. The inputs were
  // validated in base class so we can use them to compute directly.
  void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs)
      override;

 private:
  class ComputeResources;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<ComputeResources> compute_resources_;

  base::WeakPtrFactory<GraphImplCoreml> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_GRAPH_IMPL_COREML_H_
