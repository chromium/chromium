// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_NN_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_NN_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkImplNN : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImplNN();
  ~NeuralNetworkImplNN() override;

  // static
  static void Create(mojo::PendingReceiver<ml::mojom::NeuralNetwork> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<NeuralNetworkImplNN>(),
                                std::move(receiver));
  }

  void CreateModel(CreateModelCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplNN);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_NN_H_
