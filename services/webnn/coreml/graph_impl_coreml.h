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
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::coreml {

class BufferContent;
class ContextImplCoreml;

// GraphImplCoreml inherits from WebNNGraphImpl to represent a CoreML graph
// implementation. It is mainly responsible for building and compiling a CoreML
// graph from mojom::GraphInfo via GraphBuilderCoreml, then initializing and
// executing the graph. Mac OS 13.0+ is required for model compilation
// https://developer.apple.com/documentation/coreml/mlmodel/3931182-compilemodel
// Mac OS 14.0+ is required to support WebNN logical binary operators because
// the cast operator does not support casting to uint8 prior to Mac OS 14.0.
// CoreML returns bool tensors for logical operators which need to be cast to
// uint8 tensors to match WebNN expectations.
class API_AVAILABLE(macos(14.0)) GraphImplCoreml final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      ContextImplCoreml* context,
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      mojom::CreateContextOptionsPtr context_options,
      ContextProperties context_properties,
      WebNNContextImpl::CreateGraphImplCallback callback);

  GraphImplCoreml(const GraphImplCoreml&) = delete;
  GraphImplCoreml& operator=(const GraphImplCoreml&) = delete;
  ~GraphImplCoreml() override;

 private:
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

  // Parameters needed to construct a `GraphImplCoreml`. Used for shuttling
  // these objects between the background thread where the model is compiled and
  // the originating thread.
  struct Params {
    Params(
        ComputeResourceInfo compute_resource_info,
        base::flat_map<std::string, std::string> coreml_name_to_operand_name);
    ~Params();

    ComputeResourceInfo compute_resource_info;
    base::flat_map<std::string, std::string> coreml_name_to_operand_name;

    // Represents the compiled and configured Core ML model. This member must be
    // set before these params are used to construct a new `GraphImplCoreml`.
    MLModel* __strong ml_model;
  };

  GraphImplCoreml(ContextImplCoreml* context, std::unique_ptr<Params> params);

  static MLFeatureValue* CreateMultiArrayFeatureValueFromBytes(
      MLMultiArrayConstraint* multi_array_constraint,
      mojo_base::BigBuffer data);

  // Compile the CoreML model to a temporary .modelc file.
  static void CreateAndBuildOnBackgroundThread(
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
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

  static void DidCreateAndBuild(
      base::WeakPtr<WebNNContextImpl> context,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<Params>, mojom::ErrorPtr> result);

  // Execute the compiled platform graph asynchronously. The `named_inputs` were
  // validated in base class so we can use them to compute directly, the result
  // of execution will be returned to renderer process with the `callback`.
  void ComputeImpl(
      base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
      mojom::WebNNGraph::ComputeCallback callback) override;

  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override;

 private:
  void DidPredictFromCompute(base::ElapsedTimer model_predict_timer,
                             mojom::WebNNGraph::ComputeCallback callback,
                             id<MLFeatureProvider> output_features,
                             NSError* error);

  void DoDispatch(
      base::flat_map<std::string,
                     scoped_refptr<QueueableResourceState<BufferContent>>>
          named_input_buffer_states,
      base::flat_map<std::string,
                     scoped_refptr<QueueableResourceState<BufferContent>>>
          named_output_buffer_states,
      base::OnceClosure completion_closure);

  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<std::string, std::string> coreml_name_to_operand_name_;
  MLModel* __strong ml_model_;

  base::WeakPtrFactory<GraphImplCoreml> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_GRAPH_IMPL_COREML_H_
