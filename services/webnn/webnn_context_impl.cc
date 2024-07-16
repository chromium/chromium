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
#include "services/webnn/webnn_buffer_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContextClient> client_remote,
    WebNNContextProviderImpl* context_provider,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options,
    base::UnguessableToken context_handle)
    // TODO(crbug.com/345352987): pass token by value to WebNNObjectImpl.
    : WebNNObjectImpl(std::move(context_handle)),
      receiver_(this, std::move(receiver)),
      client_remote_(std::move(client_remote)),
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

void WebNNContextImpl::CreateGraph(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto compute_resource_info =
      WebNNGraphImpl::ValidateGraph(properties_, *graph_info);
  if (!compute_resource_info.has_value()) {
    receiver_.ReportBadMessage(kBadMessageInvalidGraph);
    return;
  }

  CreateGraphImpl(std::move(graph_info), *std::move(compute_resource_info),
                  base::BindOnce(&WebNNContextImpl::DidCreateWebNNGraphImpl,
                                 AsWeakPtr(), std::move(callback)));
}

void WebNNContextImpl::CreateBuffer(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // The token is validated in mojo traits to be non-empty.
  CHECK(!buffer_handle.is_empty());

  // It is illegal to create the same buffer twice, a buffer is uniquely
  // identified by its UnguessableToken.
  if (buffer_impls_.contains(buffer_handle)) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // TODO(crbug.com/40278771): handle error using MLContext.
  std::unique_ptr<WebNNBufferImpl> buffer_impl = CreateBufferImpl(
      std::move(receiver), std::move(buffer_info), buffer_handle);
  if (!buffer_impl) {
    receiver_.ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Associates a `WebNNBuffer` instance with this context so the WebNN service
  // can access the implementation.
  buffer_impls_.emplace(std::move(buffer_impl));
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
    mojom::WebNNContext::CreateGraphCallback callback,
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

void WebNNContextImpl::OnLost(const std::string& message) {
  client_remote_->OnLost(message);
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
  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
  backend_context_properties.data_type_limits.gather_indices.RetainAll(
      DataTypeConstraint::kGatherOperatorIndexDataTypes);
  backend_context_properties.data_type_limits.where_condition.RetainAll(
      {OperandDataType::kUint8});
  return backend_context_properties;
}

}  // namespace webnn
