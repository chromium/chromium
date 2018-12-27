// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl.h"

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_impl.h"

namespace ml {

ModelImpl::ModelImpl() = default;
ModelImpl::~ModelImpl() = default;

void ModelImpl::Finish(mojom::ModelInfoPtr model_info,
                       FinishCallback callback) {
  DLOG(INFO) << "ModelImpl::Finish";

  model_info_ = std::move(model_info);

  std::move(callback).Run(mojom::NOT_ERROR);
}

void ModelImpl::CreateCompilation(CreateCompilationCallback callback) {
  DLOG(INFO) << "ModelImpl::CreateCompilation";
  auto init_params = mojom::CompilationInitParams::New();

  auto model_info = mojom::ModelInfo::New();
  for (auto itr = model_info_->operands.begin();
       itr != model_info_->operands.end(); ++itr) {
    model_info->operands.push_back(itr->Clone());
  }
  for (auto itr = model_info_->operations.begin();
       itr != model_info_->operations.end(); ++itr) {
    model_info->operations.push_back(itr->Clone());
  }
  for (auto itr = model_info_->values.begin(); itr != model_info_->values.end();
       ++itr) {
    model_info->values.insert({itr->first, itr->second->Clone()});
  }
  model_info->memory = model_info_->memory->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  model_info->memory_size = model_info_->memory_size;
  model_info->inputs = model_info_->inputs;
  model_info->outputs = model_info_->outputs;

  auto impl = std::make_unique<CompilationImpl>(std::move(model_info));

  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl), mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
