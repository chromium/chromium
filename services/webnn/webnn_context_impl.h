// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNBufferImpl;
class WebNNContextProviderImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextImpl
    : public mojom::WebNNContext,
      public WebNNObjectImpl {
 public:
  using CreateGraphImplCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr>)>;

  WebNNContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                   mojo::PendingRemote<mojom::WebNNContextClient> client_remote,
                   WebNNContextProviderImpl* context_provider,
                   ContextProperties properties,
                   mojom::CreateContextOptionsPtr options,
                   base::UnguessableToken context_handle);

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

  // Disassociates a `WebNNBuffer` instance owned by this context by its handle.
  // Called when a `WebNNBuffer` instance has a connection error. After this
  // call, it is no longer safe to use the WebNNBufferImpl.
  void DisconnectAndDestroyWebNNBufferImpl(
      const base::UnguessableToken& handle);

  // Retrieves a `WebNNBufferImpl` instance created from this context.
  // Emits a bad message if a buffer with the given handle does not exist.
  base::optional_ref<WebNNBufferImpl> GetWebNNBufferImpl(
      const base::UnguessableToken& handle);

  // Get context properties with op support limits that are intersection
  // between WebNN generic limits and backend specific limits.
  static ContextProperties IntersectWithBaseProperties(
      ContextProperties backend_context_properties);

  const ContextProperties& properties() { return properties_; }
  const mojom::CreateContextOptions& options() const { return *options_; }

  void OnLost(const std::string& context_lost_info);

 protected:
  void OnConnectionError();

  // mojom::WebNNContext
  void CreateGraph(mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override;

  void CreateBuffer(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  // This method will be called by `CreateGraph()` after the graph info is
  // validated. A backend subclass should implement this method to build and
  // compile a platform specific graph asynchronously.
  virtual void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      CreateGraphImplCallback callback) = 0;

  void DidCreateWebNNGraphImpl(
      CreateGraphCallback callback,
      base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr> result);

  // This method will be called by `CreateBuffer()` after the buffer info is
  // validated. A backend subclass should implement this method to create and
  // initialize a platform specific buffer.
  virtual std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<mojom::WebNNContext> receiver_;
  mojo::Remote<mojom::WebNNContextClient> client_remote_;

  // Owns this object.
  raw_ptr<WebNNContextProviderImpl> context_provider_;

  // Context properties reported to the renderer process.
  const ContextProperties properties_;

  // Configuration options provided by the renderer process when creating this
  // context.
  mojom::CreateContextOptionsPtr options_;

  // BufferImpls must be stored on the context to allow the WebNN service to
  // identify and use them from the renderer process in MLContext operations.
  // This cache only contains valid BufferImpls whose size is managed by the
  // lifetime of the buffers it contains.
  base::flat_set<std::unique_ptr<WebNNBufferImpl>,
                 WebNNObjectImpl::Comparator<WebNNBufferImpl>>
      buffer_impls_;

 private:
  // GraphsImpls which are stored on the context to allow graph
  // operations to use this context safely via a raw_ptr.
  mojo::UniqueAssociatedReceiverSet<mojom::WebNNGraph> graph_impls_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
