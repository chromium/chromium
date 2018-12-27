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

CompilationImpl::CompilationImpl(mojom::ModelInfoPtr model_info) {
  model_info_ = std::move(model_info);
}

CompilationImpl::~CompilationImpl() {}

void CompilationImpl::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImpl::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;

  delegate_ = std::make_unique<CompilationDelegateClDnn>(this);
  int32_t result = delegate_->Compile();
  std::move(callback).Run(result);
}

void CompilationImpl::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImpl::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();
  auto remote_init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  for (size_t i = 0; i < model_info_->inputs.size(); ++i) {
    const mojom::OperandPtr& operand =
        model_info_->operands[model_info_->inputs[i]];
    input_memory_size += GetRequiredSize(operand);
    init_params->inputs.push_back(mojom::OperandInfo::New(
        model_info_->inputs[i], operand->type, operand->dimensions));
    remote_init_params->inputs.push_back(mojom::OperandInfo::New(
        model_info_->inputs[i], operand->type, operand->dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  for (size_t i = 0; i < model_info_->outputs.size(); ++i) {
    const mojom::OperandPtr& operand =
        model_info_->operands[model_info_->outputs[i]];
    output_memory_size += GetRequiredSize(operand);
    init_params->outputs.push_back(mojom::OperandInfo::New(
        model_info_->outputs[i], operand->type, operand->dimensions));
    remote_init_params->outputs.push_back(mojom::OperandInfo::New(
        model_info_->outputs[i], operand->type, operand->dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  uint32_t total_memory_size = input_memory_size + output_memory_size;
  init_params->memory = mojo::SharedBufferHandle::Create(total_memory_size);

  remote_init_params->memory = init_params->memory->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(delegate_->CreateExecution(std::move(init_params)),
                          mojo::MakeRequest(&ptr_info));
  remote_init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(remote_init_params));
}

int32_t CompilationImpl::GetScalarInt32(uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  auto mapping = model_info_->memory->MapAtOffset(info->length, info->offset);
  return reinterpret_cast<int32_t*>(mapping.get())[0];
}

float CompilationImpl::GetScalarFloat(uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  auto mapping = model_info_->memory->MapAtOffset(info->length, info->offset);
  return reinterpret_cast<float*>(mapping.get())[0];
}

mojo::ScopedSharedBufferMapping CompilationImpl::MapMemory(
    uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  return model_info_->memory->MapAtOffset(info->length, info->offset);
}

}  // namespace ml
