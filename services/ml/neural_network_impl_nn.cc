// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_nn.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/model_impl_nn.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

// static
void NeuralNetworkImplNN::Create(ml::mojom::NeuralNetworkRequest request) {
  mojo::MakeStrongBinding(std::make_unique<NeuralNetworkImplNN>(),
                          std::move(request));
}

NeuralNetworkImplNN::NeuralNetworkImplNN() = default;

NeuralNetworkImplNN::~NeuralNetworkImplNN() = default;

void NeuralNetworkImplNN::CreateModel(CreateModelCallback callback) {
  LOG(INFO) << "CreateModel";

  mojom::ModelPtrInfo model_ptr_info;
  mojo::MakeStrongBinding(std::make_unique<ModelImplNN>(),
                          mojo::MakeRequest(&model_ptr_info));

  auto init_params = mojom::ModelInitParams::New();
  init_params->model = std::move(model_ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
