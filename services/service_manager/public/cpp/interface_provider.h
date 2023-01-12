// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/export.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace service_manager {

// Encapsulates a mojo::PendingRemote|Remote<mojom::InterfaceProvider>
// implemented in a remote application. Provides two main features:
// - a typesafe GetInterface() method for binding Interface remotes.
// - a testing API that allows local callbacks to be registered that bind
//   requests for remote interfaces.
// An instance of this class is used by the GetInterface() methods on
// Connection.
class SERVICE_MANAGER_PUBLIC_CPP_EXPORT InterfaceProvider {
 public:
  class TestApi {
   public:
    explicit TestApi(InterfaceProvider* provider) : provider_(provider) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

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
    raw_ptr<InterfaceProvider, DanglingUntriaged> provider_;
  };

  // Constructs an InterfaceProvider which is usable immediately despite not
  // being bound to any actual remote implementation. Must call Bind()
  // eventually in order for the provider to function properly.
  // The task_runner argument is used for mojo remote connection.
  explicit InterfaceProvider(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Constructs an InterfaceProvider which uses |interface_provider| to issue
  // remote interface requests.
  // The task_runner argument is used for mojo remote connection.
  InterfaceProvider(
      mojo::PendingRemote<mojom::InterfaceProvider> interface_provider,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  InterfaceProvider(const InterfaceProvider&) = delete;
  InterfaceProvider& operator=(const InterfaceProvider&) = delete;

  ~InterfaceProvider();

  // Closes the currently bound mojo::PendingRemote<InterfaceProvider> for this
  // object, allowing it to be rebound to a new
  // mojo::PendingRemote<InterfaceProvider>.
  void Close();

  // Binds this InterfaceProvider to an actual mojom::InterfaceProvider pipe.
  void Bind(mojo::PendingRemote<mojom::InterfaceProvider> interface_provider);

  // Sets a closure to be run when the remote InterfaceProvider pipe is closed.
  void SetConnectionLostClosure(base::OnceClosure connection_lost_closure);

  base::WeakPtr<InterfaceProvider> GetWeakPtr();

  // Binds a passed in pending receiver to an implementation of the interface in
  // the remote application. The mojo remote associated to the interface in the
  // local application can immediately be used to start sending requests to the
  // remote interface. Uses templated parameters in order to work with weak
  // interfaces in blink.
  template <typename Interface>
  void GetInterface(mojo::PendingReceiver<Interface> receiver) {
    GetInterfaceByName(Interface::Name_, receiver.PassPipe());
  }
  void GetInterfaceByName(const std::string& name,
                          mojo::ScopedMessagePipeHandle request_handle);

 private:
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

  mojo::Remote<mojom::InterfaceProvider> interface_provider_;
  mojo::PendingReceiver<mojom::InterfaceProvider> pending_receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<InterfaceProvider> weak_factory_{this};
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_H_
