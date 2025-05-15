// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_tflite.h"

#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/graph_impl_tflite.h"
#include "services/webnn/tflite/tensor_impl_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

ContextImplTflite::ContextImplTflite(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GraphBuilderTflite::GetContextProperties(),
                       std::move(options)) {}

ContextImplTflite::~ContextImplTflite() = default;

base::WeakPtr<WebNNContextImpl> ContextImplTflite::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplTflite::CreateGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    CreateGraphImplCallback callback) {
  std::move(callback).Run(GraphImplTflite::CreateAndBuild(
      std::move(receiver), std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(constant_tensor_operands), this));
}

void ContextImplTflite::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  // TODO(crbug.com/332350952): implement constant tensors for TFLite.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    std::move(callback).Run(base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported.")));
    return;
  }
  std::move(callback).Run(TensorImplTflite::Create(std::move(receiver), this,
                                                   std::move(tensor_info)));
}

}  // namespace webnn::tflite
