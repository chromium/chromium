// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_android.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace ml {

CompilationImplAndroid::CompilationImplAndroid(const ModelImplAndroid* model)
    : operands_(model->operands_),
      operations_(model->operations_),
      inputs_(model->inputs_),
      outputs_(model->outputs_) {
  int32_t result =
      ANeuralNetworksCompilation_create(model->nn_model_, &nn_compilation_);
  DLOG(INFO) << "ANeuralNetworksCompilation_create: " << result;
}

CompilationImplAndroid::~CompilationImplAndroid() {
  ANeuralNetworksCompilation_free(nn_compilation_);
  DLOG(INFO) << "ANeuralNetworksCompilation_free";
}

void CompilationImplAndroid::Finish(int32_t preference,
                                    FinishCallback callback) {
  DLOG(INFO) << "CompilationImplAndroid::finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;

  int32_t result =
      ANeuralNetworksCompilation_setPreference(nn_compilation_, preference);
  DLOG(INFO) << "ANeuralNetworksCompilation_setPreference: " << result;
  if (result != ANEURALNETWORKS_NO_ERROR) {
    std::move(callback).Run(result);
    return;
  }

  result = ANeuralNetworksCompilation_finish(nn_compilation_);
  DLOG(INFO) << "ANeuralNetworksCompilation_finish: " << result;

  std::move(callback).Run(result);
}

void CompilationImplAndroid::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplAndroid::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  init_params->inputs.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    Operand operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(inputs_[i], operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  init_params->outputs.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    Operand operand = operands_[outputs_[i]];
    output_memory_size += operand.requiredSize();
    init_params->outputs.push_back(
        mojom::OperandInfo::New(outputs_[i], operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  uint32_t total_memory_size = input_memory_size + output_memory_size;
  mojo::ScopedSharedBufferHandle memory_handle =
      mojo::SharedBufferHandle::Create(total_memory_size);

  init_params->memory =
      memory_handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(
      std::make_unique<ExecutionImplAndroid>(this, std::move(memory_handle)),
      mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
