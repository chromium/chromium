// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

// Generates unique IDs for WebNNContextImpl.
base::AtomicSequenceNumber g_next_route_id;

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options)
    : context_provider_(context_provider),
      properties_(IntersectWithBaseProperties(std::move(properties))),
      options_(std::move(options)),
      command_buffer_id_(
          gpu::CommandBufferIdFromChannelAndRoute(context_provider->client_id(),
                                                  g_next_route_id.GetNext())),
      sequence_id_(context_provider_->scheduler()->CreateSequence(
          gpu::SchedulingPriority::kNormal,
          context_provider->main_thread_task_runner(),
          gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE,
          command_buffer_id_)),
      scheduler_task_runner_(base::MakeRefCounted<gpu::SchedulerTaskRunner>(
          *context_provider_->scheduler(),
          sequence_id_)),
      receiver_(this, std::move(receiver)) {
  CHECK(context_provider_);
  // Safe to use base::Unretained because the context_provider_ owns this class
  // that won't be destroyed until this callback executes.
  receiver_.set_disconnect_handler(
      base::BindPostTask(scheduler_task_runner_,
                         base::BindOnce(&WebNNContextImpl::OnConnectionError,
                                        base::Unretained(this))));
}

WebNNContextImpl::~WebNNContextImpl() {
  // Note: ShutDown() prevents new tasks from being scheduled and drops existing
  // ones from executing.
  scheduler_task_runner_->ShutDown();
  context_provider_->scheduler()->DestroySequence(sequence_id_);
}

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
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_impls_.emplace(std::move(graph_impl));
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
    mojo_base::BigBuffer tensor_data,
    mojom::WebNNContext::CreateTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateTensor(properties_, tensor_info->descriptor).has_value()) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    const base::expected<OperandDescriptor, std::string> validated_descriptor =
        webnn::OperandDescriptor::Create(
            properties_, tensor_info->descriptor.data_type(),
            tensor_info->descriptor.shape(), "WebNNGraphConstant");
    if (!validated_descriptor.has_value()) {
      receiver_.ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    if (!properties_.data_type_limits.constant.Has(
            validated_descriptor->data_type())) {
      receiver_.ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    if (tensor_data.size() != validated_descriptor->PackedByteLength()) {
      receiver_.ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }
  }

  mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote;
  auto receiver = remote.InitWithNewEndpointAndPassReceiver();
  scheduler_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebNNContextImpl::CreateTensorImpl, AsWeakPtr(), std::move(receiver),
          std::move(tensor_info),
          base::BindOnce(&WebNNContextImpl::DidCreateWebNNTensorImpl,
                         AsWeakPtr(), std::move(callback), std::move(remote),
                         std::move(tensor_data))));
}

void WebNNContextImpl::DidCreateWebNNTensorImpl(
    mojom::WebNNContext::CreateTensorCallback callback,
    mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote,
    mojo_base::BigBuffer tensor_data,
    base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateTensorResult::NewError(std::move(result.error())));
    return;
  }

  // Write the specified values into the tensor. If `tensor_data` is empty,
  // the tensor should be left initialized to zero. The `tensor_data` size
  // should of been already validated in CreateTensor().
  if (tensor_data.size() > 0) {
    result.value()->WriteTensorImpl(std::move(tensor_data));
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

void WebNNContextImpl::DisconnectAndDestroyWebNNGraphImpl(
    const blink::WebNNGraphToken& handle) {
  const auto it = graph_impls_.find(handle);
  CHECK(it != graph_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNGraphImpl`.
  graph_impls_.erase(it);
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
  // A specific maximum rank is still under discussion, but 8 is the highest
  // supported by any backend.
  constexpr SupportedRanks kNonScalarMaxRank = SupportedRanks::NonScalarUpTo(8);

  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
  backend_context_properties.data_type_limits.batch_normalization_input
      .data_types.RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.batch_normalization_mean
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.conv2d_input.ranks.IntersectWith(
      SupportedRanks::Exactly(4));
  backend_context_properties.data_type_limits.conv2d_bias.ranks.IntersectWith(
      SupportedRanks::Exactly(1));
  backend_context_properties.data_type_limits.conv_transpose2d_input.ranks
      .IntersectWith(SupportedRanks::Exactly(4));
  backend_context_properties.data_type_limits.conv_transpose2d_bias.ranks
      .IntersectWith(SupportedRanks::Exactly(1));
  backend_context_properties.data_type_limits.logical_not_input.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.logical_output.RetainAll(
      DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.abs_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.ceil_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.cos_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.cumulative_sum_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32Ints32To64, kNonScalarMaxRank});
  backend_context_properties.data_type_limits.dequantize_linear_input.data_types
      .RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
  backend_context_properties.data_type_limits.dequantize_linear_scale.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.dequantize_linear_zero_point
      .data_types.RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
  backend_context_properties.data_type_limits.erf_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.exp_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.floor_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.log_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.neg_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.reciprocal_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sign_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.sin_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sqrt_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tan_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.elu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gather_input.ranks.IntersectWith(
      SupportedRanks::NonScalarUpTo(8));
  backend_context_properties.data_type_limits.gather_indices.data_types
      .RetainAll(DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gather_elements_input.ranks
      .IntersectWith(SupportedRanks::NonScalarUpTo(8));
  backend_context_properties.data_type_limits.gather_elements_indices
      .IntersectWith(
          {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
           SupportedRanks::NonScalarUpTo(8)});
  backend_context_properties.data_type_limits.gather_nd_input.ranks
      .IntersectWith(SupportedRanks::NonScalarUpTo(8));
  backend_context_properties.data_type_limits.gather_nd_indices.IntersectWith(
      {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       SupportedRanks::NonScalarUpTo(8)});
  backend_context_properties.data_type_limits.gelu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gemm_a.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gemm_c.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(2)});
  backend_context_properties.data_type_limits.gru_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)});
  backend_context_properties.data_type_limits.gru_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gru_cell_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gru_cell_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.hard_sigmoid_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.hard_swish_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.instance_normalization_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.instance_normalization_scale
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.layer_normalization_input
      .data_types.RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.leaky_relu_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.linear_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.lstm_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)});
  backend_context_properties.data_type_limits.lstm_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.lstm_cell_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.lstm_cell_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.matmul_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, {2, 8}});
  backend_context_properties.data_type_limits.pad_input.IntersectWith(
      {SupportedDataTypes::All(), kNonScalarMaxRank});
  backend_context_properties.data_type_limits.average_pool2d_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.l2_pool2d_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.max_pool2d_input.IntersectWith(
      {SupportedDataTypes::All(), SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.prelu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.quantize_linear_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.quantize_linear_zero_point
      .data_types.RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
  backend_context_properties.data_type_limits.reduce_l1_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_l2_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_exp_input
      .data_types.RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_mean_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_product_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_square_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.relu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.resample2d_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.scatter_elements_input.ranks
      .IntersectWith(SupportedRanks::NonScalarUpTo(8));
  backend_context_properties.data_type_limits.scatter_elements_indices
      .data_types.RetainAll(
          DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.scatter_nd_input.ranks
      .IntersectWith(SupportedRanks::NonScalarUpTo(8));
  backend_context_properties.data_type_limits.scatter_nd_indices.IntersectWith(
      {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       SupportedRanks::NonScalarUpTo(8)});
  backend_context_properties.data_type_limits.sigmoid_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.slice_input.IntersectWith(
      {SupportedDataTypes::All(), kNonScalarMaxRank});
  backend_context_properties.data_type_limits.softmax_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softplus_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softsign_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tanh_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.triangular_input.IntersectWith(
      {SupportedDataTypes::All(), {2, 8}});
  backend_context_properties.data_type_limits.where_condition.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  return backend_context_properties;
}

}  // namespace webnn
