// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_

#include <optional>
#include <set>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_pending_constant_operand.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

class WebNNConstantOperand;
class WebNNContextImpl;
class WebNNTensorImpl;

// Services-side connection to an `MLGraphBuilder`. Responsible for managing
// data associated with the graph builder.
//
// A `WebNNGraphBuilderImpl` may create at most one `WebNNGraphImpl`, when
// `CreateGraph()` is called. Once built, this graph does not depend on its
// builder and the builder will be destroyed.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphBuilderImpl
    : public mojom::WebNNGraphBuilder {
 public:
  explicit WebNNGraphBuilderImpl(WebNNContextImpl& context);

  WebNNGraphBuilderImpl(const WebNNGraphBuilderImpl&) = delete;
  WebNNGraphBuilderImpl& operator=(const WebNNGraphBuilderImpl&) = delete;

  ~WebNNGraphBuilderImpl() override;

  // mojom::WebNNGraphBuilder
  void CreatePendingConstant(
      const blink::WebNNPendingConstantToken& constant_handle,
      OperandDataType data_type,
      mojo_base::BigBuffer data) override;

  void CreateGraph(mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override;
  void IsValidGraphForTesting(const ContextProperties& context_properties,
                              mojom::GraphInfoPtr graph_info,
                              IsValidGraphForTestingCallback callback) override;

  void SetId(mojo::ReceiverId id, base::PassKey<WebNNContextImpl> pass_key);

 protected:
  struct ValidateGraphSuccessResult {
    ValidateGraphSuccessResult(
        WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
        base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
            constant_operands,
        base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands);
    ~ValidateGraphSuccessResult();

    ValidateGraphSuccessResult(const ValidateGraphSuccessResult&) = delete;
    ValidateGraphSuccessResult& operator=(const ValidateGraphSuccessResult&) =
        delete;

    ValidateGraphSuccessResult(ValidateGraphSuccessResult&&);
    ValidateGraphSuccessResult& operator=(ValidateGraphSuccessResult&&);

    WebNNGraphImpl::ComputeResourceInfo compute_resource_info;

    // Constant operands associated with this graph, which will be used during
    // graph construction. This member is only non-empty when
    // `keep_builder_resources_for_testing` is false.
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands;

    // Constant tensors associated with this graph, which will be used during
    // graph construction.
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands;
  };

  // Transfer ownership of this builder's resources to a returned
  // `ValidateGraphSuccessResult` which may be used to construct a
  // `WebNNGraphImpl` if `graph_info` is valid; otherwise return null.
  //
  // `keep_builder_resources_for_testing` must only be true in tests. Otherwise
  // this method may be called at most once.
  [[nodiscard]] std::optional<ValidateGraphSuccessResult> ValidateGraphImpl(
      const ContextProperties& context_properties,
      const mojom::GraphInfo& graph_info,
      bool keep_builder_resources_for_testing);

 private:
  void DidTransposePendingPermutations(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      CreateGraphCallback callback,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&&
          constant_operands);
  void DidCreateGraph(
      CreateGraphCallback callback,
      mojo::PendingAssociatedRemote<mojom::WebNNGraph> remote,
      base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr> result);

  void DestroySelf();

  SEQUENCE_CHECKER(sequence_checker_);

  // The `WebNNContextImpl` which owns and will outlive this object.
  const raw_ref<WebNNContextImpl> context_;

  // Set by the owning `context_` so this builder can identify itself when
  // requesting to be destroyed.
  mojo::ReceiverId id_;

  // Tracks whether `CreateGraph()` has been called. If so, any subsequent
  // incoming messages to the mojo pipe are signs of a misbehaving renderer.
  bool has_built_ = false;

  std::set<std::unique_ptr<WebNNPendingConstantOperand>,
           WebNNPendingConstantOperand::Comparator>
      pending_constant_operands_;

  base::WeakPtrFactory<WebNNGraphBuilderImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
