// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_WIN_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_WIN_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom.h"

namespace ml {

class ModelImplWin;

class NeuralNetworkImplWin : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImplWin();
  ~NeuralNetworkImplWin() override;

  void CreateModel(CreateModelCallback callback) override;

  static void Create(mojom::NeuralNetworkRequest request);

 private:
  friend class ModelImplWin;

  mojo::StrongBindingPtr<mojom::NeuralNetwork> binding_;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplWin);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_WIN_H_
