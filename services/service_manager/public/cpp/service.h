// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Service {
 public:
  Service();
  virtual ~Service();

  // Transfers ownership of |service| to itself such that self-termination via
  // |Terminate()| is also self-deletion. Note that most services implicitly
  // call |Terminate()| when disconnected from the Service Manager, via the
  // default implementation of |OnDisconnected()|.
  //
  // This should really only be called on a Service instance that has a bound
  // connection to the Service Manager, e.g. a functioning ServiceReceiver. If
  // the service never calls |Terminate()|, it will effectively leak.
  //
  // If |callback| is non-null, it will be invoked after |service| is destroyed.
  static void RunAsyncUntilTermination(std::unique_ptr<Service> service,
                                       base::OnceClosure callback = {});

  // Sets a closure to run when the service wants to self-terminate. This may be
  // used by whomever created the Service instance in order to clean up
  // associated resources.
  void set_termination_closure(base::OnceClosure callback) {
    termination_closure_ = std::move(callback);
  }

  // Called exactly once when a bidirectional connection with the Service
  // Manager has been established. No calls to OnBindInterface() will be made
  // before this.
  virtual void OnStart();

  // Called when the service instance identified by |source.identity| requests
  // to have a receiver for |interface_name| connected through the Service
  // Manager, and the Service Manager routes the request to |this|. By the time
  // this method has been called, the Service Manager has already determined
  // that policy allows for such a connection to be fulfilled.
  //
  // |receiver_pipe| is a message pipe handle that can be used to construct a
  // mojo::PendingReceiver<T>, where T should dynamically correspond to the
  // interface named by |interface_name|. Services can use a BinderMap to
  // simplify the work of mapping incoming requests to methods which bind
  // specific types of interfaces.
  //
  // NOTE: Do not override |OnBindInterface()| if overriding this method. This
  // method always takes precedence, so |OnBindInterface()| will never be called
  // if this is overridden.
  virtual void OnConnect(const ConnectSourceInfo& source,
                         const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle receiver_pipe);

  // DEPRECATED: Same as above, but deprecated naming. Prefer to override
  // |OnConnect()| instead. In any case, do not override both!
  virtual void OnBindInterface(const BindSourceInfo& source,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);

  // Called by the Service Manager when it wants this service to launch a new
  // instance of a packaged service listed in this service's Manifest. The
  // packaged service to launch is identified by |service_name|, and the
  // Service receiver pipe in |service_receiver| should be used to construct
  // a new Service instance for the packaged service. If and when that instance
  // is created, |callback| should be invoked with the new instance's PID (which
  // may be the same as this service's PID if they will share a process). If the
  // requested service is not launched, |callback| should be invoked with
  // |std::nullopt|.
  using CreatePackagedServiceInstanceCallback =
      base::OnceCallback<void(std::optional<base::ProcessId>)>;
  virtual void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<mojom::Service> service_receiver,
      CreatePackagedServiceInstanceCallback callback);

  // Called when the Service Manager has stopped tracking this instance. Once
  // invoked, no further Service interface methods will be called on this
  // Service, and no further communication with the Service Manager is possible.
  //
  // The Service may continue to operate and service existing client connections
  // as it deems appropriate. The default implementation invokes |Terminate()|.
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

  // Runs a RunLoop until this service self-terminates. This is intended for use
  // in environments where the service is the only thing running, e.g. as a
  // standalone executable.
  void RunUntilTermination();

 protected:
  // Subclasses should always invoke |Terminate()| when they want to
  // self-terminate. This should generally only be done once the service is
  // disconnected from the Service Manager and has no outstanding interface
  // connections servicing clients. Calling |Terminate()| should be considered
  // roughly equivalent to calling |exit(0)| in a normal POSIX process
  // environment, except that services allow for the host environment to define
  // exactly what termination means (see |set_termination_closure| above).
  //
  // Note that if no termination closure is set on this Service instance,
  // calls to |Terminate()| do nothing.
  //
  // As a general rule, subclasses should *ALWAYS* assume that |Terminate()| may
  // delete |*this| before returning.
  void Terminate();

 private:
  base::OnceClosure termination_closure_;

};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
