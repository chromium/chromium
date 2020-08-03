// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_NEURAL_NETWORK_IMPL_MAC_H_
#define SERVICES_ML_NEURAL_NETWORK_IMPL_MAC_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/ml/neural_network_impl.h"

namespace ml {

class NeuralNetworkImplMac : public NeuralNetworkImpl {
 public:
  NeuralNetworkImplMac();
  ~NeuralNetworkImplMac() override;

  // static
  static void Create(mojo::PendingReceiver<ml::mojom::NeuralNetwork> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<NeuralNetworkImplMac>(),
                                std::move(receiver));
  }

  void CreateModel(CreateModelCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkImplMac);
};

}  // namespace ml

#endif  // SERVICES_ML_NEURAL_NETWORK_IMPL_MAC_H_