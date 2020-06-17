// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_nn.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace ml {

CompilationImplNN::CompilationImplNN(const ModelImplNN* model, mojom::ModelInfoPtr model_info,
      mojo::ScopedSharedBufferMapping mapping)
    : operands_(model->operands_),
      operations_(model->operations_),
      inputs_(model->inputs_),
      outputs_(model->outputs_),
      model_info_(std::move(model_info)),
      mapping_(std::move(mapping)) {
#if defined(OS_ANDROID)
  int32_t result =
      ANeuralNetworksCompilation_create(model->nn_model_, &nn_compilation_);
#else
  int32_t result =
      ie_compilation_create(model->ie_model_, &ie_compilation_);
#endif
  DLOG(INFO) << "ANeuralNetworksCompilation_create: " << result;
}

CompilationImplNN::~CompilationImplNN() {
  // ANeuralNetworksCompilation_free(nn_compilation_);
  // The nn_compilation_ will be deleted in execution phase.
}

void CompilationImplNN::Finish(int32_t preference,
                                    FinishCallback callback) {
  DLOG(INFO) << "CompilationImplNN::finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;

#if defined(OS_ANDROID)
  int32_t result =
      ANeuralNetworksCompilation_setPreference(nn_compilation_, preference);
#else
  int32_t result = ie_compilation_set_preference(ie_compilation_, preference);
#endif
  if (result != 0) {
    std::move(callback).Run(result);
    return;
  }

#if defined(OS_ANDROID)
  result = ANeuralNetworksCompilation_finish(nn_compilation_);
#else
  result = ie_compilation_finish(ie_compilation_);
#endif
  DLOG(INFO) << "ANeuralNetworksCompilation_finish: " << result;

  std::move(callback).Run(result);
}

void CompilationImplNN::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplNN::CreateExecution";
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
      std::make_unique<ExecutionImplNN>(this, std::move(memory_handle)),
      mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
