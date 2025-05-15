// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/context_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "base/sequence_checker.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/graph_impl_coreml.h"
#include "services/webnn/coreml/tensor_impl_coreml.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::coreml {

ContextImplCoreml::ContextImplCoreml(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GraphBuilderCoreml::GetContextProperties(),
                       std::move(options)) {}

ContextImplCoreml::~ContextImplCoreml() = default;

base::WeakPtr<WebNNContextImpl> ContextImplCoreml::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplCoreml::CreateGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    CreateGraphImplCallback callback) {
  GraphImplCoreml::CreateAndBuild(
      std::move(receiver), this, std::move(graph_info),
      std::move(compute_resource_info), std::move(constant_operands),
      std::move(constant_tensor_operands), options().Clone(), properties(),
      std::move(callback));
}

void ContextImplCoreml::CreateTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    mojom::TensorInfoPtr tensor_info,
    CreateTensorImplCallback callback) {
  // TODO(crbug.com/332350952): implement constant tensors for CoreML.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    std::move(callback).Run(base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Creation of constant tensors is not supported.")));
    return;
  }
  std::move(callback).Run(TensorImplCoreml::Create(std::move(receiver), this,
                                                   std::move(tensor_info)));
}

}  // namespace webnn::coreml
