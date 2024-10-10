// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "base/sequence_checker.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options)
    : receiver_(this, std::move(receiver)),
      context_provider_(context_provider),
      properties_(IntersectWithBaseProperties(std::move(properties))),
      options_(std::move(options)) {
  CHECK(context_provider_);
  // Safe to use base::Unretained because the context_provider_ owns this class
  // that won't be destroyed until this callback executes.
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebNNContextImpl::OnConnectionError, base::Unretained(this)));
}

WebNNContextImpl::~WebNNContextImpl() = default;

void WebNNContextImpl::OnConnectionError() {
  context_provider_->OnConnectionError(this);
}

#if DCHECK_IS_ON()
void WebNNContextImpl::AssertCalledOnValidSequence() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
#endif

void WebNNContextImpl::ReportBadGraphBuilderMessage(
    const std::string& message,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_builder_impls_.ReportBadMessage(message);
}

void WebNNContextImpl::TakeGraph(
    std::unique_ptr<WebNNGraphImpl> graph_impl,
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> graph_pending_receiver,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_impls_.Add(std::move(graph_impl), std::move(graph_pending_receiver));
}

void WebNNContextImpl::RemoveGraphBuilder(
    mojo::ReceiverId graph_builder_id,
    base::PassKey<WebNNGraphBuilderImpl> /*pass_key*/) {
  graph_builder_impls_.Remove(graph_builder_id);
}

void WebNNContextImpl::CreateGraphBuilder(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraphBuilder> receiver) {
  auto graph_builder = std::make_unique<WebNNGraphBuilderImpl>(*this);
  WebNNGraphBuilderImpl* graph_builder_ptr = graph_builder.get();

  mojo::ReceiverId id =
      graph_builder_impls_.Add(std::move(graph_builder), std::move(receiver));

  graph_builder_ptr->SetId(id, base::PassKey<WebNNContextImpl>());
}

void WebNNContextImpl::CreateTensor(
    mojom::TensorInfoPtr tensor_info,
    mojom::WebNNContext::CreateTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateTensor(properties_, tensor_info->descriptor).has_value()) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote;
  auto receiver = remote.InitWithNewEndpointAndPassReceiver();
  CreateTensorImpl(
      std::move(receiver), std::move(tensor_info),
      base::BindOnce(&WebNNContextImpl::DidCreateWebNNTensorImpl, AsWeakPtr(),
                     std::move(callback), std::move(remote)));
}

void WebNNContextImpl::DidCreateWebNNTensorImpl(
    mojom::WebNNContext::CreateTensorCallback callback,
    mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote,
    base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateTensorResult::NewError(std::move(result.error())));
    return;
  }

  auto success = mojom::CreateTensorSuccess::New(std::move(remote),
                                                 result.value()->handle());
  std::move(callback).Run(
      mojom::CreateTensorResult::NewSuccess(std::move(success)));

  // Associates a `WebNNTensor` instance with this context so the WebNN service
  // can access the implementation.
  tensor_impls_.emplace(*std::move(result));
}

void WebNNContextImpl::DisconnectAndDestroyWebNNTensorImpl(
    const blink::WebNNTensorToken& handle) {
  const auto it = tensor_impls_.find(handle);
  CHECK(it != tensor_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNTensorImpl`.
  tensor_impls_.erase(it);
}

void WebNNContextImpl::ResetReceiverWithReason(std::string_view message) {
  receiver_.ResetWithReason(/*custom_reason_code=*/0, message);
}

void WebNNContextImpl::OnLost(std::string_view message) {
  ResetReceiverWithReason(message);
  context_provider_->OnConnectionError(this);
}

base::optional_ref<WebNNTensorImpl> WebNNContextImpl::GetWebNNTensorImpl(
    const blink::WebNNTensorToken& tensor_handle) {
  const auto it = tensor_impls_.find(tensor_handle);
  if (it == tensor_impls_.end()) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return std::nullopt;
  }
  return it->get();
}

ContextProperties WebNNContextImpl::IntersectWithBaseProperties(
    ContextProperties backend_context_properties) {
  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
  backend_context_properties.data_type_limits.batch_normalization_input
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.logical_not_input.RetainAll(
      DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.logical_output.RetainAll(
      DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.abs_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.ceil_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.cos_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.cumulative_sum_input.RetainAll(
      DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.dequantize_linear_input.RetainAll(
      DataTypeConstraint::kInts4ToInts8);
  backend_context_properties.data_type_limits.dequantize_linear_scale.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.erf_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.exp_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.floor_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.log_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.neg_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.reciprocal_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sign_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.sin_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sqrt_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tan_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.elu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gather_indices.RetainAll(
      DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gather_elements_indices.RetainAll(
      DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gather_nd_indices.RetainAll(
      DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gelu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gemm_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gru_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gru_cell_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.hard_sigmoid_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.hard_swish_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.instance_normalization_input
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.layer_normalization_input
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.leaky_relu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.linear_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.lstm_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.lstm_cell_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.matmul_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.average_pool2d_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.l2_pool2d_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.prelu_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.quantize_linear_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.quantize_linear_zero_point
      .RetainAll(DataTypeConstraint::kInts4ToInts8);
  backend_context_properties.data_type_limits.reduce_l1_input.RetainAll(
      DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_l2_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_exp_input
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_mean_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_product_input.RetainAll(
      DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_input.RetainAll(
      DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_square_input.RetainAll(
      DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.relu_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.resample2d_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.scatter_nd_indices.RetainAll(
      DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.sigmoid_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softmax_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softplus_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softsign_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tanh_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.where_condition.RetainAll(
      DataTypeConstraint::kUint8);
  return backend_context_properties;
}

}  // namespace webnn
