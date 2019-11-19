// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
#define CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace content {

// Helper for handling incoming RunService requests on UtilityThreadImpl.
class UtilityServiceFactory {
 public:
  UtilityServiceFactory();
  ~UtilityServiceFactory();

  void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

 private:
  std::unique_ptr<service_manager::Service> CreateAudioService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  // Allows embedders to register their interface implementations before the
  // network or audio services are created. Used for testing.
  std::unique_ptr<service_manager::BinderRegistry> network_registry_;
  std::unique_ptr<service_manager::BinderMap> audio_binders_;

  DISALLOW_COPY_AND_ASSIGN(UtilityServiceFactory);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
