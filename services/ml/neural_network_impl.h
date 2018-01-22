// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_H_

#include "services/ml/public/interfaces/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkImpl {
 public:
  static void Create(ml::mojom::NeuralNetworkRequest request);

 private:
  ~NeuralNetworkImpl() = default;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImpl);
};

}  // namespace  

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_H_