// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_win.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/execution_impl_win.h"
#include "services/ml/model_impl_win.h"

namespace ml {

CompilationImplWin::CompilationImplWin(const ModelImplWin* model)
    : model_(model), program_(nullptr) {
  operands_ = model->operands_;
  operations_ = model->operations_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
}

CompilationImplWin::~CompilationImplWin() {
  cldnn_status status;
  if (program_) {
    cldnn_release_program(program_, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release program " << status << " "
                  << std::string(cldnn_get_last_error_message());
    }
  }
  DLOG(INFO) << "[clDNN] succeed to release program";
}

void CompilationImplWin::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImplWin::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;

  cldnn_status status;
  std::vector<cldnn_build_option> build_options;
  program_ =
      cldnn_build_program(model_->engine_, model_->topology_,
                          build_options.data(), build_options.size(), &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to build program " << status << " "
                << std::string(cldnn_get_last_error_message());
    std::move(callback).Run(mojom::OP_FAILED);
    return;
  }
  DLOG(INFO) << "[clDNN] succeed to build program";

  std::move(callback).Run(mojom::NOT_ERROR);
}

void CompilationImplWin::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplWin::CreateExecution";
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
      std::make_unique<ExecutionImplWin>(this, std::move(memory_handle)),
      mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
