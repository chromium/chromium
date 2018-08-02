// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_android.h"
#include "services/ml/model_impl_android.h"

namespace ml {

void NeuralNetworkImplAndroid::Create(
    ml::mojom::NeuralNetworkRequest request) {
  auto impl = std::make_unique<NeuralNetworkImplAndroid>();
  auto* impl_ptr = impl.get();
  impl_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

NeuralNetworkImplAndroid::NeuralNetworkImplAndroid() {}

NeuralNetworkImplAndroid::~NeuralNetworkImplAndroid() {}

void NeuralNetworkImplAndroid::createModel(createModelCallback callback) {
  LOG(INFO) << "createModel";
  auto init_params = mojom::ModelInitParams::New();

  auto model_impl = std::make_unique<ModelImplAndroid>();

  mojom::ModelPtrInfo model_ptr_info;
  mojo::MakeStrongBinding(std::move(model_impl),
                          mojo::MakeRequest(&model_ptr_info));
  init_params->model = std::move(model_ptr_info);
  
  std::move(callback).Run(mojom::NOT_ERROR,
                          std::move(init_params));
}

}  // namespace ml
