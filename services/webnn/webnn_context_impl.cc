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
#include "services/webnn/webnn_buffer_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"

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

void WebNNContextImpl::CreateBuffer(
    mojom::BufferInfoPtr buffer_info,
    mojom::WebNNContext::CreateBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateBuffer(properties_, buffer_info->descriptor).has_value()) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  mojo::PendingAssociatedRemote<mojom::WebNNBuffer> remote;
  auto receiver = remote.InitWithNewEndpointAndPassReceiver();
  CreateBufferImpl(
      std::move(receiver), std::move(buffer_info),
      base::BindOnce(&WebNNContextImpl::DidCreateWebNNBufferImpl, AsWeakPtr(),
                     std::move(callback), std::move(remote)));
}

void WebNNContextImpl::DidCreateWebNNBufferImpl(
    mojom::WebNNContext::CreateBufferCallback callback,
    mojo::PendingAssociatedRemote<mojom::WebNNBuffer> remote,
    base::expected<std::unique_ptr<WebNNBufferImpl>, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateBufferResult::NewError(std::move(result.error())));
    return;
  }

  auto success = mojom::CreateBufferSuccess::New(std::move(remote),
                                                 result.value()->handle());
  std::move(callback).Run(
      mojom::CreateBufferResult::NewSuccess(std::move(success)));

  // Associates a `WebNNBuffer` instance with this context so the WebNN service
  // can access the implementation.
  buffer_impls_.emplace(*std::move(result));
}

void WebNNContextImpl::DisconnectAndDestroyWebNNBufferImpl(
    const blink::WebNNBufferToken& handle) {
  const auto it = buffer_impls_.find(handle);
  CHECK(it != buffer_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNBufferImpl`.
  buffer_impls_.erase(it);
}

void WebNNContextImpl::OnLost(std::string_view message) {
  receiver_.ResetWithReason(/*custom_reason_code=*/0, message);
  context_provider_->OnConnectionError(this);
}

base::optional_ref<WebNNBufferImpl> WebNNContextImpl::GetWebNNBufferImpl(
    const blink::WebNNBufferToken& buffer_handle) {
  const auto it = buffer_impls_.find(buffer_handle);
  if (it == buffer_impls_.end()) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return std::nullopt;
  }
  return it->get();
}

ContextProperties WebNNContextImpl::IntersectWithBaseProperties(
    ContextProperties backend_context_properties) {
  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
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
  backend_context_properties.data_type_limits.sin_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sqrt_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tan_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.elu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gather_indices.RetainAll(
      DataTypeConstraint::kGatherIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gelu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.leaky_relu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.relu_input.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To32);
  backend_context_properties.data_type_limits.sigmoid_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softmax_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softplus_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softsign_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.where_condition.RetainAll(
      DataTypeConstraint::kUint8);
  return backend_context_properties;
}

}  // namespace webnn
