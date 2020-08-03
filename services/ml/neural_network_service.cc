// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_service.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "services/ml/ml_switches.h"
#if defined(OS_LINUX) || defined(OS_WIN)
#include "services/ml/neural_network_impl.h"
#include "services/ml/neural_network_impl_nn.h"
#elif defined(OS_ANDROID)
#include "services/ml/neural_network_impl_nn.h"
#elif defined(OS_MACOSX)
#include "services/ml/neural_network_impl_mac.h"
#endif

namespace ml {

NeuralNetworkService::NeuralNetworkService(
      mojo::PendingReceiver<mojom::NeuralNetworkService> receiver)
    : receiver_(this, std::move(receiver)) {}

NeuralNetworkService::~NeuralNetworkService() = default;

void NeuralNetworkService::BindNeuralNetwork(
      mojo::PendingReceiver<mojom::NeuralNetwork> receiver) {
#if (OS_LINUX) || defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseInferenceEngine)) {
    NeuralNetworkImplNN::Create(std::move(receiver));
  } else {
    NeuralNetworkImpl::Create(std::move(receiver));
  }
#elif defined(OS_ANDROID)
  NeuralNetworkImplNN::Create(std::move(receiver));
#elif defined(OS_MACOSX)
  NeuralNetworkImplMac::Create(std::move(receiver));
#endif
}

}  // namespace ml
