// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_LINUX_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_LINUX_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

class NeuralNetworkImplLinux
    : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImplLinux();
  ~NeuralNetworkImplLinux() override;

  void createModel(createModelCallback callback) override;

  static void Create(mojom::NeuralNetworkRequest request);
 private:
  mojo::StrongBindingPtr<mojom::NeuralNetwork> binding_;
  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplLinux);
};

}  // namespace  


#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_LINUX_H_
