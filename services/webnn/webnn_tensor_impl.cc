// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_tensor_impl.h"

#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNTensorImpl::WebNNTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl* context,
    mojom::TensorInfoPtr tensor_info)
    : context_(context),
      descriptor_(std::move(tensor_info->descriptor)),
      usage_(std::move(tensor_info->usage)),
      receiver_(this, std::move(receiver)) {
  // Safe to use base::Unretained because `this` owns `receiver_`.
  receiver_.set_disconnect_handler(
      base::BindOnce(&WebNNTensorImpl::OnDisconnect, base::Unretained(this)));
}

WebNNTensorImpl::~WebNNTensorImpl() = default;

void WebNNTensorImpl::ReadTensor(ReadTensorCallback callback) {
  if (!usage().Has(MLTensorUsageFlags::kRead)) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call ReadTensorImpl() implemented by a backend.
  ReadTensorImpl(std::move(callback));
}

void WebNNTensorImpl::WriteTensor(mojo_base::BigBuffer src_buffer) {
  if (!usage().Has(MLTensorUsageFlags::kWrite)) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // TODO(https://crbug.com/40278771): Generate error using MLContext.
  if (PackedByteLength() < src_buffer.size()) {
    receiver_.ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call WriteTensorImpl() implemented by a backend.
  WriteTensorImpl(std::move(src_buffer));
}

void WebNNTensorImpl::OnDisconnect() {
  context_->DisconnectAndDestroyWebNNTensorImpl(handle());
}

}  // namespace webnn
