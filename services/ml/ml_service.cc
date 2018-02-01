// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/ml_service.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/service_context.h"
#if defined(OS_LINUX)
#include "services/ml/neural_network_impl_linux.h"
#elif defined(OS_ANDROID)
#include "services/ml/neural_network_impl_android.h"
#else
#include "services/ml/neural_network_impl.h"
#endif

namespace ml {

std::unique_ptr<service_manager::Service> MLService::Create() {
  return std::make_unique<MLService>();
}

MLService::MLService() = default;

MLService::~MLService() = default;

void MLService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));

#if defined(OS_LINUX)
  registry_.AddInterface(base::Bind(&NeuralNetworkImplLinux::Create));
#elif defined(OS_ANDROID)
  registry_.AddInterface(base::Bind(&NeuralNetworkImplAndroid::Create));
#else
  registry_.AddInterface(base::Bind(&NeuralNetworkImpl::Create));
#endif
}

void MLService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace ml
