// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

class SessionOptions final : public base::RefCountedThreadSafe<SessionOptions> {
 public:
  static base::expected<scoped_refptr<SessionOptions>, mojom::ErrorPtr> Create(
      const mojom::CreateContextOptions::Device device_type);

  SessionOptions(const SessionOptions&) = delete;
  SessionOptions& operator=(const SessionOptions&) = delete;

  const OrtSessionOptions* get() const { return session_options_.get(); }

 private:
  friend class base::RefCountedThreadSafe<SessionOptions>;
  SessionOptions(ScopedOrtSessionOptions session_options);
  ~SessionOptions();

  ScopedOrtSessionOptions session_options_;
};

// `ContextImplOrt` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplOrt` which uses ORT for inference.
class ContextImplOrt final : public WebNNContextImpl {
 public:
  ContextImplOrt(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 WebNNContextProviderImpl* context_provider,
                 const mojom::CreateContextOptions::Device device_type,
                 mojom::CreateContextOptionsPtr options,
                 ScopedOrtEnv env,
                 scoped_refptr<SessionOptions> session_options);

  ContextImplOrt(const WebNNContextImpl&) = delete;
  ContextImplOrt& operator=(const ContextImplOrt&) = delete;

  ~ContextImplOrt() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  static ContextProperties GetContextProperties(
      const mojom::CreateContextOptions::Device device_type);

  scoped_refptr<SessionOptions> session_options() const {
    return session_options_;
  }

 private:
  void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CreateGraphImplCallback callback) override;

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override;

  ScopedOrtEnv env_;

  // The session options are shared among all the sessions created by this
  // context.
  scoped_refptr<SessionOptions> session_options_;

  base::WeakPtrFactory<ContextImplOrt> weak_factory_{this};
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
