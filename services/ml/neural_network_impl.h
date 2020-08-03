// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkImpl : public mojom::NeuralNetwork {
 public:
  NeuralNetworkImpl();
  ~NeuralNetworkImpl() override;

  // static
  static void Create(mojo::PendingReceiver<ml::mojom::NeuralNetwork> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<NeuralNetworkImpl>(),
                                std::move(receiver));
  }

  void CreateModel(CreateModelCallback callback) override;

 protected:
  friend class ModelImpl;

  mojo::StrongBindingPtr<mojom::NeuralNetwork> binding_;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_MAC_H_