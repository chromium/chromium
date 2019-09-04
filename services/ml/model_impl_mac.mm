// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_mac.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_impl_mac.h"

namespace ml {

ModelImplMac::ModelImplMac() = default;

ModelImplMac::~ModelImplMac() = default;

void ModelImplMac::CreateCompilation(CreateCompilationCallback callback) {
  auto init_params = mojom::CompilationInitParams::New();

  auto model_info = mojom::ModelInfo::New();
  CopyModelInfo(model_info);
  auto impl = std::make_unique<CompilationImplMac>(std::move(model_info));

  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl), mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml