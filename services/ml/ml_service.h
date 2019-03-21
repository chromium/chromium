// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_SERVICE_H_
#define SERVICES_ML_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace ml {

class MLService : public service_manager::Service {
 public:
  explicit MLService(service_manager::mojom::ServiceRequest request);
  ~MLService() override;

  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MLService);
};

}  // namespace ml

#endif  // SERVICES_ML_SERVICE_H_
