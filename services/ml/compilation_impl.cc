// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/compilation_delegate_cl_dnn.h"
#include "services/ml/model_impl.h"

namespace ml {

CompilationImpl::CompilationImpl(const ModelImpl* model) {
  operands_ = model->operands_;
  operations_ = model->operations_;
  values_ = model->values_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
  memory_size_ = model->memory_size_;
  memory_.reset(new int8_t[memory_size_]);
  memcpy(memory_.get(), model->memory_.get(), memory_size_);
}

CompilationImpl::~CompilationImpl() {}

void CompilationImpl::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImpl::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;

  delegate_ = std::make_unique<CompilationDelegateClDnn>();

  int32_t result = delegate_->Init(this);
  if (result != mojom::NOT_ERROR) {
    std::move(callback).Run(result);
    return;
  }

  result = delegate_->Compile();
  std::move(callback).Run(mojom::NOT_ERROR);
}

void CompilationImpl::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImpl::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    Operand operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  for (size_t i = 0; i < outputs_.size(); ++i) {
    Operand operand = operands_[outputs_[i]];
    output_memory_size += operand.requiredSize();
    init_params->outputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  uint32_t total_memory_size = input_memory_size + output_memory_size;
  mojo::ScopedSharedBufferHandle memory_handle =
      mojo::SharedBufferHandle::Create(total_memory_size);

  init_params->memory =
      memory_handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(
      delegate_->CreateExecution(std::move(memory_handle)),
      mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
