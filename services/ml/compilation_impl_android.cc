// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_android.h"

namespace ml {

CompilationImplAndroid::CompilationImplAndroid(ModelImplAndroid* model) {
  operands_ = model->operands_;
  operations_ = model->operations_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;

  int32_t result = ANeuralNetworksCompilation_create(model->nn_model_, &nn_compilation_);

  LOG(INFO) << "ANeuralNetworksCompilation_create: " << result;
}
CompilationImplAndroid::~CompilationImplAndroid() {
  ANeuralNetworksCompilation_free(nn_compilation_);
  LOG(INFO) << "ANeuralNetworksCompilation_free";
}

void CompilationImplAndroid::setPreference(int32_t preference, setPreferenceCallback callback) {
  LOG(INFO) << "CompilationImplAndroid::setPreference";
  LOG(INFO) << "  " << "preference: " << preference;

  // TODO: convert the blink preference to NN API types.
  int32_t result = ANeuralNetworksCompilation_setPreference(nn_compilation_, preference);

  LOG(INFO) << "ANeuralNetworksCompilation_setPreference: " << result;

  std::move(callback).Run(result);
}

void CompilationImplAndroid::finish(finishCallback callback) {
  LOG(INFO) << "CompilationImplAndroid::finish";

  int32_t result = ANeuralNetworksCompilation_finish(nn_compilation_);

  LOG(INFO) << "ANeuralNetworksCompilation_finish: " << result;

  std::move(callback).Run(result);
}

void CompilationImplAndroid::createExecution(createExecutionCallback callback) {
  LOG(INFO) << "CompilationImplAndroid::createExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  auto impl = std::make_unique<ExecutionImplAndroid>(this);

  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
