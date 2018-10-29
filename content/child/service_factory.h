// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_FACTORY_H_
#define CONTENT_CHILD_SERVICE_FACTORY_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"

namespace service_manager {
class EmbeddedServiceRunner;
}

namespace content {

// Base class for child-process specific implementations of
// service_manager::mojom::ServiceFactory.
class ServiceFactory : public service_manager::mojom::ServiceFactory {
 public:
  using ServiceMap =
      std::map<std::string, service_manager::EmbeddedServiceInfo>;

  ServiceFactory();
  ~ServiceFactory() override;

  virtual void RegisterServices(ServiceMap* services) = 0;
  virtual void OnServiceQuit() {}

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override;

 private:
  // Called if CreateService fails to find a registered service.
  virtual void OnLoadFailed() {}

  bool has_registered_services_ = false;
  std::unordered_map<std::string,
                     std::unique_ptr<service_manager::EmbeddedServiceRunner>>
      services_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_FACTORY_H_
