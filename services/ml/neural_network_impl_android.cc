// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_android.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/model_impl_android.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

// static
void NeuralNetworkImplAndroid::Create(ml::mojom::NeuralNetworkRequest request) {
  mojo::MakeStrongBinding(std::make_unique<NeuralNetworkImplAndroid>(),
                          std::move(request));
}

NeuralNetworkImplAndroid::NeuralNetworkImplAndroid() = default;

NeuralNetworkImplAndroid::~NeuralNetworkImplAndroid() = default;

void NeuralNetworkImplAndroid::CreateModel(CreateModelCallback callback) {
  LOG(INFO) << "CreateModel";

  mojom::ModelPtrInfo model_ptr_info;
  mojo::MakeStrongBinding(std::make_unique<ModelImplAndroid>(),
                          mojo::MakeRequest(&model_ptr_info));

  auto init_params = mojom::ModelInitParams::New();
  init_params->model = std::move(model_ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
