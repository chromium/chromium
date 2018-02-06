// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_linux.h"

namespace ml {

CompilationImplLinux::CompilationImplLinux(ModelImplLinux* model) {
  operands_ = model->operands_;
  operations_ = model->operations_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
}
CompilationImplLinux::~CompilationImplLinux() {}

void CompilationImplLinux::setPreference(int32_t preference, setPreferenceCallback callback) {
  DLOG(INFO) << "CompilationImplLinux::setPreference";
  DLOG(INFO) << "  " << "preference: " << preference;
  std::move(callback).Run(mojom::NO_ERROR);
}

void CompilationImplLinux::finish(finishCallback callback) {
  DLOG(INFO) << "CompilationImplLinux::finish";
  std::move(callback).Run(mojom::NO_ERROR);
}

void CompilationImplLinux::createExecution(createExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplLinux::createExecution";
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
  
  init_params->memory = memory_handle->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  auto impl = std::make_unique<ExecutionImplLinux>(this, std::move(memory_handle));
  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
