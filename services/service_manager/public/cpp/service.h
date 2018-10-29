// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

class ServiceContext;
struct BindSourceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Service {
 public:
  Service();
  virtual ~Service();

  // Called exactly once when a bidirectional connection with the Service
  // Manager has been established. No calls to OnBindInterface() will be made
  // before this.
  virtual void OnStart();

  // Called when the service identified by |source.identity| requests this
  // service bind a request for |interface_name|. If this method has been
  // called, the service manager has already determined that policy permits this
  // interface to be bound, so the implementation of this method can trust that
  // it should just blindly bind it under most conditions.
  virtual void OnBindInterface(const BindSourceInfo& source,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);

  // Called when the Service Manager has stopped tracking this instance. Once
  // invoked, no further Service interface methods will be called on this
  // Service, and no further communication with the Service Manager is possible.
  //
  // The Service may continue to operate and service existing client connections
  // as it deems appropriate.
  virtual void OnDisconnected();

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to shut down, and in fact its process
  // may be reaped shortly afterward if applicable.
  //
  // If this returns |true| then QuitNow() will be invoked immediately upon
  // return to the ServiceContext. Otherwise the Service is responsible for
  // eventually calling QuitNow().
  //
  // The default implementation returns |true|.
  //
  // NOTE: This may be called at any time, and once it's been called, none of
  // the other public Service methods will be invoked by the ServiceContext.
  //
  // This is ONLY invoked when using a ServiceContext and is therefore
  // deprecated.
  virtual bool OnServiceManagerConnectionLost();

 protected:
  // Accesses the ServiceContext associated with this Service. Note that this is
  // only valid AFTER the Service's constructor has run.
  ServiceContext* context() const;

 private:
  friend class ForwardingService;
  friend class ServiceContext;
  friend class TestServiceDecorator;

  // NOTE: This MUST be called before any public Service methods. ServiceContext
  // satisfies this guarantee for any Service instance it owns.
  virtual void SetContext(ServiceContext* context);

  ServiceContext* service_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

// TODO(rockot): Remove this. It's here to satisfy a few remaining use cases
// where a Service impl is owned by something other than its ServiceContext.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ForwardingService : public Service {
 public:
  // |target| must outlive this object.
  explicit ForwardingService(Service* target);
  ~ForwardingService() override;

  // Service:
  void OnStart() override;
  void OnBindInterface(const BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

 private:
  void SetContext(ServiceContext* context) override;

  Service* const target_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForwardingService);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
