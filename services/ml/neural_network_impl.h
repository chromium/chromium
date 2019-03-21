// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkImpl : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImpl();
  ~NeuralNetworkImpl() override;

  void CreateModel(CreateModelCallback callback) override;

  static void Create(mojom::NeuralNetworkRequest request);

 private:
  friend class ModelImpl;

  mojo::StrongBindingPtr<mojom::NeuralNetwork> binding_;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_H_