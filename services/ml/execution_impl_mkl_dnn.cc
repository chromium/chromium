// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mkl_dnn.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_delegate_mkl_dnn.h"

namespace ml {

ExecutionImplMklDnn::ExecutionImplMklDnn(
    std::shared_ptr<CompiledModelMklDnn> compiled_model,
    mojom::ExecutionInitParamsPtr params)
    : params_(std::move(params)),
      compiled_model_(std::move(compiled_model)) {
}

ExecutionImplMklDnn::~ExecutionImplMklDnn() {
}

void ExecutionImplMklDnn::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMklDnn::StartCompute";

  uint32_t total_length = 0;
  for (size_t i = 0; i < params_->inputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->inputs[i];
    const uint32_t offset = total_length;
    const uint32_t length = GetRequiredSize(operand);
    total_length += length;
    if (operand->type != mojom::TENSOR_FLOAT32) {
      DLOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
    auto mapping = params_->memory->MapAtOffset(length, offset);
    DLOG(INFO) << "Mapping " << mapping.get() << " for input " << i
               << " offset " << offset << " length " << length;
  }

  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    // Use the reordered outputs (byxf).
    const mojom::OperandInfoPtr& operand = params_->outputs[i];
    const uint32_t offset = total_length;
    const uint32_t length = GetRequiredSize(operand);
    total_length += length;
    auto mapping = params_->memory->MapAtOffset(length, offset);
    DLOG(INFO) << "Mapping " << mapping.get() << " for output " << i
               << " offset " << offset << " length " << length;
  }

  std::move(callback).Run(mojom::NOT_ERROR);
}

}  // namespace ml
