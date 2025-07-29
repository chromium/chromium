// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_tensor_impl.h"

#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNTensorImpl::WebNNTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNReceiverImpl<mojom::WebNNTensor>(std::move(receiver),
                                            context->scheduler_task_runner()),
      context_(std::move(context)),
      descriptor_(std::move(tensor_info->descriptor)),
      usage_(std::move(tensor_info->usage)) {}

WebNNTensorImpl::WebNNTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    std::unique_ptr<gpu::WebNNTensorRepresentation> representation)
    : WebNNReceiverImpl<mojom::WebNNTensor>(std::move(receiver),
                                            context->scheduler_task_runner()),
      context_(std::move(context)),
      representation_(std::move(representation)),
      descriptor_(std::move(tensor_info->descriptor)),
      usage_(std::move(tensor_info->usage)) {}

WebNNTensorImpl::~WebNNTensorImpl() = default;

bool WebNNTensorImpl::IsValidWithDescriptor(
    const OperandDescriptor& descriptor) const {
  return descriptor_ == descriptor;
}

void WebNNTensorImpl::ReadTensor(ReadTensorCallback callback) {
  if (!usage().Has(MLTensorUsageFlags::kRead)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call ReadTensorImpl() implemented by a backend.
  PostTaskToOwningTaskRunner(base::BindOnce(&WebNNTensorImpl::ReadTensorImpl,
                                            this, std::move(callback)));
}

void WebNNTensorImpl::WriteTensor(mojo_base::BigBuffer src_buffer) {
  if (!usage().Has(MLTensorUsageFlags::kWrite)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // TODO(https://crbug.com/40278771): Generate error using MLContext.
  if (PackedByteLength() < src_buffer.size()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call WriteTensorImpl() implemented by a backend.
  PostTaskToOwningTaskRunner(base::BindOnce(&WebNNTensorImpl::WriteTensorImpl,
                                            this, std::move(src_buffer)));
}

void WebNNTensorImpl::OnDisconnect() {
  context_->RemoveWebNNTensorImpl(handle());
}

}  // namespace webnn
