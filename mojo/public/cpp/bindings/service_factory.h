// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
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
// Any time |MaybeRunService()| is called on the ServiceFactory, it will match
// the GenericPendingReceiver argument's interface type against the list of
// factories it has available, and if it finds a match it will run that function
// and retain ownership of the returned object until the corresponding receiver
// is disconnected.
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
//     void HandleServiceRequest(mojo::GenericPendingReceiver receiver) {
//       static base::NoDestructor<mojo::ServiceFactory> factory{
//         RunFooService,
//         RunBarService,
//       };
//
//       if (!factory->MaybeRunService(&receiver)) {
//         // The receiver was for neither the Foo nor Bar service. Sad!
//         LOG(ERROR) << "Unknown service: " << *receiver.interface_name();
//       }
//     }
//
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ServiceFactory {
 public:
  template <typename... Funcs>
  explicit ServiceFactory(Funcs... fns)
      : callbacks_({base::BindRepeating(&RunFunction<Funcs>, fns)...}) {}
  ~ServiceFactory();

  // Attempts to run a service supported by this factory.
  //
  // Returns |true| and consumes |*receiver| if it is a suitable match for some
  // function known by the factory; otherwise returns |false| and leaves
  // |*receiver| intact.
  bool MaybeRunService(GenericPendingReceiver* receiver);

 private:
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InstanceHolderBase {
   public:
    InstanceHolderBase();
    virtual ~InstanceHolderBase();

    void WatchPipe(MessagePipeHandle pipe,
                   base::OnceClosure disconnect_callback);

   private:
    void OnDisconnect(MojoResult result, const HandleSignalsState& state);

    SimpleWatcher watcher_;
    base::OnceClosure disconnect_callback_;

    DISALLOW_COPY_AND_ASSIGN(InstanceHolderBase);
  };

  template <typename Interface>
  class InstanceHolder : public InstanceHolderBase {
   public:
    explicit InstanceHolder(std::unique_ptr<Interface> instance)
        : instance_(std::move(instance)) {}
    ~InstanceHolder() override = default;

   private:
    const std::unique_ptr<Interface> instance_;

    DISALLOW_COPY_AND_ASSIGN(InstanceHolder);
  };

  template <typename Func>
  static std::unique_ptr<InstanceHolderBase> RunFunction(
      Func fn,
      GenericPendingReceiver* receiver) {
    using Interface = typename internal::ServiceFactoryTraits<Func>::Interface;
    if (auto typed_receiver = receiver->As<Interface>()) {
      return std::make_unique<InstanceHolder<Interface>>(
          fn(std::move(typed_receiver)));
    }
    return nullptr;
  }

  void OnInstanceDisconnected(InstanceHolderBase* instance);

  using GenericCallback =
      base::RepeatingCallback<std::unique_ptr<InstanceHolderBase>(
          GenericPendingReceiver*)>;
  const std::vector<GenericCallback> callbacks_;

  base::flat_set<std::unique_ptr<InstanceHolderBase>, base::UniquePtrComparator>
      instances_;

  base::WeakPtrFactory<ServiceFactory> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

namespace internal {

template <typename Impl, typename InterfaceType>
struct ServiceFactoryTraits<std::unique_ptr<Impl> (*)(
    PendingReceiver<InterfaceType>)> {
  using Interface = InterfaceType;
};

}  // namespace internal

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SERVICE_FACTORY_H_
