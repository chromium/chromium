// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_

#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/export.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace service_manager {

// Encapsulates a mojom::InterfaceProviderPtr implemented in a remote
// application. Provides two main features:
// - a typesafe GetInterface() method for binding InterfacePtrs.
// - a testing API that allows local callbacks to be registered that bind
//   requests for remote interfaces.
// An instance of this class is used by the GetInterface() methods on
// Connection.
class SERVICE_MANAGER_PUBLIC_CPP_EXPORT InterfaceProvider {
 public:
  using ForwardCallback =
      base::RepeatingCallback<void(const std::string&,
                                   mojo::ScopedMessagePipeHandle)>;
  class TestApi {
   public:
    explicit TestApi(InterfaceProvider* provider) : provider_(provider) {}
    ~TestApi() {}

    void SetBinderForName(
        const std::string& name,
        base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder) {
      provider_->SetBinderForName(name, std::move(binder));
    }

    bool HasBinderForName(const std::string& name) {
      return provider_->HasBinderForName(name);
    }

    void ClearBinderForName(const std::string& name) {
      provider_->ClearBinderForName(name);
    }

    void ClearBinders() {
      provider_->ClearBinders();
    }

   private:
    InterfaceProvider* provider_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Constructs an InterfaceProvider which is usable immediately despite not
  // being bound to any actual remote implementation. Must call Bind()
  // eventually in order for the provider to function properly.
  InterfaceProvider();

  // Constructs an InterfaceProvider which uses |interface_provider| to issue
  // remote interface requests.
  explicit InterfaceProvider(mojom::InterfaceProviderPtr interface_provider);

  ~InterfaceProvider();

  // Closes the currently bound InterfaceProviderPtr for this object, allowing
  // it to be rebound to a new InterfaceProviderPtr.
  void Close();

  // Binds this InterfaceProvider to an actual mojom::InterfaceProvider pipe.
  // It is an error to call this on a forwarding InterfaceProvider, i.e. this
  // call is exclusive to Forward().
  void Bind(mojom::InterfaceProviderPtr interface_provider);

  // Sets this InterfaceProvider to forward all GetInterface() requests to
  // |callback|. It is an error to call this on a bound InterfaceProvider, i.e.
  // this call is exclusive to Bind(). In addition, and unlike Bind(), this MUST
  // be called before any calls to GetInterface() are made.
  void Forward(const ForwardCallback& callback);

  // Sets a closure to be run when the remote InterfaceProvider pipe is closed.
  void SetConnectionLostClosure(base::OnceClosure connection_lost_closure);

  base::WeakPtr<InterfaceProvider> GetWeakPtr();

  // Binds a passed in interface pointer to an implementation of the interface
  // in the remote application using MakeRequest. The interface pointer can
  // immediately be used to start sending requests to the remote interface.
  // Uses templated parameters in order to work with weak interfaces in blink.
  template <typename... Args>
  void GetInterface(Args&&... args) {
    GetInterface(MakeRequest(std::forward<Args>(args)...));
  }
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request) {
    GetInterfaceByName(Interface::Name_, std::move(request.PassMessagePipe()));
  }
  template <typename Interface>
  void GetInterface(mojo::PendingReceiver<Interface> receiver) {
    GetInterfaceByName(Interface::Name_, receiver.PassPipe());
  }
  void GetInterfaceByName(const std::string& name,
                          mojo::ScopedMessagePipeHandle request_handle);

  // Returns a callback to GetInterface<Interface>(). This can be passed to
  // BinderRegistry::AddInterface() to forward requests.
  template <typename Interface>
  base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)>
  CreateInterfaceFactory() {
    return base::BindRepeating(
        &InterfaceProvider::BindInterfaceRequestFromSource<Interface>,
        GetWeakPtr());
  }

 private:
  template <typename Interface>
  void BindInterfaceRequestFromSource(
      mojo::InterfaceRequest<Interface> request) {
    GetInterface<Interface>(std::move(request));
  }

  void SetBinderForName(
      const std::string& name,
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder) {
    binders_[name] = std::move(binder);
  }
  bool HasBinderForName(const std::string& name) const;
  void ClearBinderForName(const std::string& name);
  void ClearBinders();

  using BinderMap =
      std::map<std::string,
               base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>>;
  BinderMap binders_;

  mojom::InterfaceProviderPtr interface_provider_;
  mojom::InterfaceProviderRequest pending_request_;

  // A callback to receive all GetInterface() requests in lieu of the
  // InterfaceProvider pipe.
  ForwardCallback forward_callback_;

  base::WeakPtrFactory<InterfaceProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterfaceProvider);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_
