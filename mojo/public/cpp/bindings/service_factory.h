// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace mojo {

namespace internal {
template <typename Func>
struct ServiceFactoryTraits;
}

// ServiceFactory is a helper that Mojo consumers can use to conveniently handle
// dynamic service interface requests via a GenericPendingReceiver by matching
// the receiver's interface type against a series of strongly-typed factory
// function pointers, each with the signature:
//
//   std::unique_ptr<T>(mojo::PendingReceiver<Interface>)
//
// where |T| is any type (generally an implementation of |Interface|), and
// |Interface| is a mojom interface.
//
// Any time |RunService()| is called on the ServiceFactory, it will match the
// GenericPendingReceiver argument's interface type against the list of
// factories it has available and run the corresponding function, retaining
// ownership of the returned object until the corresponding receiver is
// disconnected.
//
// Typical usage might look something like:
//
//     auto RunFooService(mojo::PendingReceiver<foo::mojom::Foo> receiver) {
//       return std::make_unique<foo::FooImpl>(std::move(receiver));
//     }
//
//     auto RunBarService(mojo::PendingReceiver<bar::mojom::Bar> receiver) {
//       return std::make_unique<bar::BarImpl>(std::move(receiver));
//     }
//
//     void RegisterServices(mojo::ServiceFactory& services) {
//       services.Add(RunFooService);
//       services.Add(RunBarService);
//     }
//
//     void HandleServiceRequest(const mojo::ServiceFactory& factory,
//                               mojo::GenericPendingReceiver receiver) {
//       if (factory.CanRunService(receiver)) {
//         factory.RunService(std::move(receiver), base::NullCallback());
//         return;
//       }
//
//       // The receiver was for neither the Foo nor Bar service. Sad!
//       LOG(ERROR) << "Unknown service: " << *receiver.interface_name();
//     }
//
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ServiceFactory {
 public:
  ServiceFactory();

  ServiceFactory(const ServiceFactory&) = delete;
  ServiceFactory& operator=(const ServiceFactory&) = delete;

  ~ServiceFactory();

  // Adds a new service to the factory. The argument may be any function that
  // accepts a single PendingReceiver<T> and returns a unique_ptr<T>, where T is
  // a service interface (that is, a generated mojom interface class
  // corresponding to some service's main interface.) Safely drops registration
  // if Interface is RuntimeFeature disabled. CanRunService() will return false
  // for these ignored Interfaces.
  template <typename Func>
  void Add(Func func) {
    using Interface = typename internal::ServiceFactoryTraits<Func>::Interface;
    if (internal::GetRuntimeFeature_IsEnabled<Interface>()) {
      constructors_[Interface::Name_] =
          base::BindRepeating(&RunConstructor<Func>, func);
    }
  }

  // If `receiver` is references an interface matching a service known to this
  // factory, this returns true. Otherwise it returns false. `receiver` MUST be
  // valid.
  bool CanRunService(const GenericPendingReceiver& receiver) const;

  // Consumes `receiver` and binds it to a new instance of the corresponding
  // service, constructed using the service's registered function within this
  // factory.
  //
  // `termination_callback`, if not null, will be invoked on the calling
  // TaskRunner whenever the new service instance is eventually destroyed.
  //
  // If the service represented by `receiver` is not known to this factory, it
  // is discarded and `termination_callback` is never run.
  bool RunService(GenericPendingReceiver receiver,
                  base::OnceClosure termination_callback);

 private:
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InstanceHolderBase {
   public:
    InstanceHolderBase();

    InstanceHolderBase(const InstanceHolderBase&) = delete;
    InstanceHolderBase& operator=(const InstanceHolderBase&) = delete;

    virtual ~InstanceHolderBase();

    void WatchPipe(MessagePipeHandle pipe,
                   base::OnceClosure disconnect_callback);

   private:
    void OnPipeSignaled(MojoResult result, const HandleSignalsState& state);

    SimpleWatcher watcher_;
    base::OnceClosure disconnect_callback_;
  };

  template <typename Impl>
  class InstanceHolder : public InstanceHolderBase {
   public:
    explicit InstanceHolder(std::unique_ptr<Impl> instance)
        : instance_(std::move(instance)) {}

    InstanceHolder(const InstanceHolder&) = delete;
    InstanceHolder& operator=(const InstanceHolder&) = delete;

    ~InstanceHolder() override = default;

   private:
    const std::unique_ptr<Impl> instance_;
  };

  template <typename Func>
  static std::unique_ptr<InstanceHolderBase> RunConstructor(
      Func fn,
      GenericPendingReceiver receiver) {
    using Interface = typename internal::ServiceFactoryTraits<Func>::Interface;
    using Impl = typename internal::ServiceFactoryTraits<Func>::Impl;
    auto impl = fn(receiver.As<Interface>());
    if (!impl)
      return nullptr;

    return std::make_unique<InstanceHolder<Impl>>(std::move(impl));
  }

  void OnInstanceDisconnected(InstanceHolderBase* instance);

  using Constructor =
      base::RepeatingCallback<std::unique_ptr<InstanceHolderBase>(
          GenericPendingReceiver)>;
  std::map<std::string, Constructor> constructors_;

  base::flat_set<std::unique_ptr<InstanceHolderBase>, base::UniquePtrComparator>
      instances_;

  base::WeakPtrFactory<ServiceFactory> weak_ptr_factory_{this};
};

namespace internal {

template <typename ImplType, typename InterfaceType>
struct ServiceFactoryTraits<std::unique_ptr<ImplType> (*)(
    PendingReceiver<InterfaceType>)> {
  using Interface = InterfaceType;
  using Impl = ImplType;
};

}  // namespace internal

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_
