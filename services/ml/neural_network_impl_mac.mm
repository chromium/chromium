// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_mac.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/model_impl_mac.h"

namespace ml {

NeuralNetworkImplMac::NeuralNetworkImplMac() = default;

NeuralNetworkImplMac::~NeuralNetworkImplMac() = default;

void NeuralNetworkImplMac::CreateModel(CreateModelCallback callback) {
  auto model_impl = std::make_unique<ModelImplMac>();

  auto init_params = mojom::ModelInitParams::New();
  mojom::ModelPtrInfo model_ptr_info;
  mojo::MakeStrongBinding(std::move(model_impl),
                          mojo::MakeRequest(&model_ptr_info));
  init_params->model = std::move(model_ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

void NeuralNetworkImplMac::Create(ml::mojom::NeuralNetworkRequest request) {
  auto impl = std::make_unique<NeuralNetworkImplMac>();
  auto* impl_ptr = impl.get();
  impl_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

}  // namespace ml