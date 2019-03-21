// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_ANDROID_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_ANDROID_H_

#include "base/macros.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkImplAndroid : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImplAndroid();
  ~NeuralNetworkImplAndroid() override;

  void CreateModel(CreateModelCallback callback) override;

  static void Create(mojom::NeuralNetworkRequest request);

 private:
  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplAndroid);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_ANDROID_H_
