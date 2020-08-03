// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_SERVICE_H_
#define SERVICES_ML_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"

namespace ml {

class NeuralNetworkService : public mojom::NeuralNetworkService {
 public:
  explicit NeuralNetworkService(
      mojo::PendingReceiver<mojom::NeuralNetworkService> receiver);
  ~NeuralNetworkService() override;

  // mojom::NeuralNetworkService implementation:
  void BindNeuralNetwork(
      mojo::PendingReceiver<mojom::NeuralNetwork> receiver) override;

 private:
  mojo::Receiver<mojom::NeuralNetworkService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(NeuralNetworkService);
};

}  // namespace ml

#endif  // SERVICES_ML_SERVICE_H_
