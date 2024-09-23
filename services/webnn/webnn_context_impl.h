// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_

#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNConstantOperand;
class WebNNGraphBuilderImpl;
class WebNNTensorImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextImpl
    : public mojom::WebNNContext,
      public WebNNObjectImpl<blink::WebNNContextToken> {
 public:
  using CreateGraphImplCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr>)>;

  using CreateTensorImplCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr>)>;

  WebNNContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                   WebNNContextProviderImpl* context_provider,
                   ContextProperties properties,
                   mojom::CreateContextOptionsPtr options);

  WebNNContextImpl(const WebNNContextImpl&) = delete;
  WebNNContextImpl& operator=(const WebNNContextImpl&) = delete;

  ~WebNNContextImpl() override;

  virtual base::WeakPtr<WebNNContextImpl> AsWeakPtr()
      VALID_CONTEXT_REQUIRED(sequence_checker_) = 0;

#if DCHECK_IS_ON()
  // Callers which obtain a WeakPtr from the method above may use this helper to
  // assert that the WeakPtr is being used correctly.
  void AssertCalledOnValidSequence() const;
#endif  // DCHECK_IS_ON()

  // Disassociates a `WebNNTensor` instance owned by this context by its handle.
  // Called when a `WebNNTensor` instance has a connection error. After this
  // call, it is no longer safe to use the WebNNTensorImpl.
  void DisconnectAndDestroyWebNNTensorImpl(
      const blink::WebNNTensorToken& handle);

  // Retrieves a `WebNNTensorImpl` instance created from this context.
  // Emits a bad message if a tensor with the given handle does not exist.
  base::optional_ref<WebNNTensorImpl> GetWebNNTensorImpl(
      const blink::WebNNTensorToken& handle);

  // Report the currently dispatching Message as bad and remove the GraphBuilder
  // receiver which received it.
  void ReportBadGraphBuilderMessage(
      const std::string& message,
      base::PassKey<WebNNGraphBuilderImpl> pass_key);

  // This method will be called by `WebNNGraphBuilderImpl::CreateGraph()` after
  // `graph_info` is validated. A backend subclass should implement this method
  // to build and compile a platform specific graph asynchronously.
  //
  // TODO(crbug.com/354724062): Move this to either `WebNNGraphImpl` or
  // `WebNNGraphBuilderImpl`.
  virtual void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CreateGraphImplCallback callback) = 0;

  // Pass ownership of a newly-created `graph_impl` to this context.
  void TakeGraph(
      std::unique_ptr<WebNNGraphImpl> graph_impl,
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> graph_pending_receiver,
      base::PassKey<WebNNGraphBuilderImpl> pass_key);

  // Called by a graph builder to destroy itself.
  void RemoveGraphBuilder(mojo::ReceiverId graph_builder_id,
                          base::PassKey<WebNNGraphBuilderImpl> pass_key);

  // Get context properties with op support limits that are intersection
  // between WebNN generic limits and backend specific limits.
  static ContextProperties IntersectWithBaseProperties(
      ContextProperties backend_context_properties);

  const ContextProperties& properties() { return properties_; }
  const mojom::CreateContextOptions& options() const { return *options_; }

  void ResetReceiverWithReason(std::string_view message);

  // Closes the `receiver_` pipe with the renderer process, then self destructs
  // by removing itself from the ownership of `context_provider_`.
  void OnLost(std::string_view context_lost_info);

  WebNNContextProviderImpl* context_provider() const {
    return context_provider_.get();
  }

 protected:
  void OnConnectionError();

  // mojom::WebNNContext
  void CreateGraphBuilder(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraphBuilder> receiver)
      override;
  void CreateTensor(mojom::TensorInfoPtr tensor_info,
                    CreateTensorCallback callback) override;

  // This method will be called by `CreateTensor()` after the tensor info is
  // validated. A backend subclass should implement this method to create and
  // initialize a platform specific tensor asynchronously.
  virtual void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) = 0;

  void DidCreateWebNNTensorImpl(
      CreateTensorCallback callback,
      mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote,
      base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr> result);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<mojom::WebNNContext> receiver_;

  // Owns this object.
  raw_ptr<WebNNContextProviderImpl> context_provider_;

  // Context properties reported to the renderer process.
  const ContextProperties properties_;

  // Configuration options provided by the renderer process when creating this
  // context.
  mojom::CreateContextOptionsPtr options_;

  // TensorImpls must be stored on the context to allow the WebNN service to
  // identify and use them from the renderer process in MLContext operations.
  // This cache only contains valid TensorImpls whose size is managed by the
  // lifetime of the tensors it contains.
  base::flat_set<
      std::unique_ptr<WebNNTensorImpl>,
      WebNNObjectImpl<blink::WebNNTensorToken>::Comparator<WebNNTensorImpl>>
      tensor_impls_;

 private:
  // Graph builders owned by this context.
  mojo::UniqueAssociatedReceiverSet<mojom::WebNNGraphBuilder>
      graph_builder_impls_;

  // GraphsImpls which are stored on the context to allow graph
  // operations to use this context safely via a raw_ptr.
  mojo::UniqueAssociatedReceiverSet<mojom::WebNNGraph> graph_impls_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
