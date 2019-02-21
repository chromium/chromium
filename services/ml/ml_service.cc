// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/ml_service.h"
#include "services/ml/ml_switches.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#if defined(OS_LINUX) || defined(OS_WIN)
#include "services/ml/neural_network_impl.h"
#elif defined(OS_ANDROID)
#include "services/ml/neural_network_impl_android.h"
#elif defined(OS_MACOSX)
#include "services/ml/neural_network_impl.h"
#include "services/ml/neural_network_impl_mac.h"
#endif

namespace ml {

MLService::MLService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

MLService::~MLService() = default;

void MLService::OnStart() {
#if defined(OS_MACOSX)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseMkldnn)) {
    registry_.AddInterface(base::Bind(&NeuralNetworkImpl::Create));
  } else {
    // for mps and bnns
    registry_.AddInterface(base::Bind(&NeuralNetworkImplMac::Create));
  }

#elif defined(OS_LINUX) || defined(OS_WIN)
  registry_.AddInterface(base::Bind(&NeuralNetworkImpl::Create));
#elif defined(OS_ANDROID)
  registry_.AddInterface(base::Bind(&NeuralNetworkImplAndroid::Create));
#endif
}

void MLService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace ml
