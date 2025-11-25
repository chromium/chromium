// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notimplemented.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"

namespace webnn::ort {

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    size_t size,
    ScopedOrtValue tensor,
    bool can_access_on_cpu)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info)),
      tensor_(std::move(tensor)),
      size_(size) {
  // Initialize the tensor with zeros, otherwise, reading uninitialized memory
  // will get random values.
  // TODO(crbug.com/461303833): check whether fast HW clears can be used
  // instead.
  if (can_access_on_cpu) {
    std::ranges::fill(AsSpan(), 0);
  }
}

TensorImplOrt::~TensorImplOrt() = default;

base::span<uint8_t> TensorImplOrt::AsSpan() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(
      PlatformFunctions::GetInstance()->ort_api()->GetTensorMutableData(
          tensor_.get(), &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  // SAFETY: ORT guarantees that it has allocated enough memory to
  // store tensor.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), size_));
}

void TensorImplOrt::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  base::span<const uint8_t> buffer_span = AsSpan();
  CHECK_EQ(PackedByteLength(), buffer_span.size());
  std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
      context_->WriteDataToDataPipeOrBigBuffer(buffer_span)));
}

void TensorImplOrt::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  context_->ReadDataFromBigBufferOrDataPipe(std::move(src_buffer), AsSpan());
}

bool TensorImplOrt::ImportTensorImpl() {
  NOTIMPLEMENTED();
  return false;
}

void TensorImplOrt::ExportTensorImpl(ScopedAccessPtr access,
                                     ExportTensorCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace webnn::ort
