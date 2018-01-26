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
  LOG(INFO) << "CompilationImplLinux::setPreference";
  LOG(INFO) << "  " << "preference: " << preference;
  std::move(callback).Run(mojom::NO_ERROR);
}

void CompilationImplLinux::finish(finishCallback callback) {
  LOG(INFO) << "CompilationImplLinux::finish";
  std::move(callback).Run(mojom::NO_ERROR);
}

void CompilationImplLinux::createExecution(createExecutionCallback callback) {
  LOG(INFO) << "CompilationImplLinux::createExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  auto impl = std::make_unique<ExecutionImplLinux>(this);

  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
