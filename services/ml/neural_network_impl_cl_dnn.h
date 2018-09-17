// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_CL_DNN_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_CL_DNN_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom.h"

namespace ml {

class ModelImplClDnn;

class NeuralNetworkImplClDnn : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImplClDnn();
  ~NeuralNetworkImplClDnn() override;

  void CreateModel(CreateModelCallback callback) override;

  static void Create(mojom::NeuralNetworkRequest request);

 private:
  friend class ModelImplClDnn;

  mojo::StrongBindingPtr<mojom::NeuralNetwork> binding_;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplClDnn);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_CL_DNN_H_
