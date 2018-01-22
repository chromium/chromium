// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl.h"

namespace ml {

// static
void NeuralNetworkImpl::Create(
    ml::mojom::NeuralNetworkRequest request) {
  DLOG(ERROR) << "Platform not supported for Neural Network Service.";
}

}  // namespace ml
