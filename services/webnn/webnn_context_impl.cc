// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
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

void WebNNContextImpl::CreateGraph(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    mojom::WebNNGraphBuilder::CreateGraphCallback callback,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateGraphImpl(std::move(graph_info), std::move(compute_resource_info),
                  base::BindOnce(&WebNNContextImpl::DidCreateWebNNGraphImpl,
                                 AsWeakPtr(), std::move(callback)));
}

void WebNNContextImpl::CreateGraphBuilder(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraphBuilder> receiver) {
  graph_builder_impls_.Add(std::make_unique<WebNNGraphBuilderImpl>(*this),
                           std::move(receiver));
}

void WebNNContextImpl::CreateBuffer(
    mojom::BufferInfoPtr buffer_info,
    mojom::WebNNContext::CreateBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    const base::UnguessableToken& handle) {
  const auto it = buffer_impls_.find(handle);
  CHECK(it != buffer_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNBufferImpl`.
  buffer_impls_.erase(it);
}

void WebNNContextImpl::DidCreateWebNNGraphImpl(
    mojom::WebNNGraphBuilder::CreateGraphCallback callback,
    base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateGraphResult::NewError(std::move(result.error())));
    return;
  }

  mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver;
  std::move(callback).Run(mojom::CreateGraphResult::NewGraphRemote(
      receiver.InitWithNewEndpointAndPassRemote()));

  graph_impls_.Add(*std::move(result), std::move(receiver));
}

void WebNNContextImpl::OnLost(std::string_view message) {
  receiver_.ResetWithReason(/*custom_reason=*/0, message);
  context_provider_->OnConnectionError(this);
}

base::optional_ref<WebNNBufferImpl> WebNNContextImpl::GetWebNNBufferImpl(
    const base::UnguessableToken& buffer_handle) {
  const auto it = buffer_impls_.find(buffer_handle);
  if (it == buffer_impls_.end()) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return std::nullopt;
  }
  return it->get();
}

ContextProperties WebNNContextImpl::IntersectWithBaseProperties(
    ContextProperties backend_context_properties) {
  static constexpr SupportedDataTypes kGatherIndicesSupportedDataTypes = {
      OperandDataType::kInt32, OperandDataType::kUint32,
      OperandDataType::kInt64};

  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
  backend_context_properties.data_type_limits.elu_input.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gather_indices.RetainAll(
      kGatherIndicesSupportedDataTypes);
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
