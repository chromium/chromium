// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_win.h"

#include <utility>

#include "services/ml/model_impl_win.h"

namespace ml {

void NeuralNetworkImplWin::Create(ml::mojom::NeuralNetworkRequest request) {
  auto impl = std::make_unique<NeuralNetworkImplWin>();
  auto* impl_ptr = impl.get();
  impl_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

NeuralNetworkImplWin::NeuralNetworkImplWin() = default;

NeuralNetworkImplWin::~NeuralNetworkImplWin() = default;

void NeuralNetworkImplWin::CreateModel(CreateModelCallback callback) {
  LOG(INFO) << "createModel";
  auto init_params = mojom::ModelInitParams::New();
  mojom::ModelPtrInfo model_ptr_info;
  mojo::MakeStrongBinding(std::make_unique<ModelImplWin>(),
                          mojo::MakeRequest(&model_ptr_info));
  init_params->model = std::move(model_ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
