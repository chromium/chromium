// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_map_internal.h"

namespace service_manager {

// Helper class which maps interface names to callbacks that know how to bind
// receivers of the corresponding interface type. This is useful for services
// which may bind many different types of interface receivers throughout their
// lifetime, given the generic name + receiver pipe handle inputs provided by
// an incoming |Service::OnConnect| message.
//
// New binders can be registered in the map by calling |Add()|. To handle an
// incoming request to bind a receiver, the owner of this BinderMap should call
// |TryBind()| with the given name and receiver pipe. If a suitable binder
// callback is registered in the map, it will be run with the receiver passed
// in.
//
// If a non-void ContextType is specified, registered callbacks must accept an
// additional ContextType argument, and each invocation of |TryBind()| must
// provide such a value.
//
// NOTE: Most common uses of BinderMapWithContext do not require a context value
// per bind request. Use the BinderMap alias defined below this class in such
// cases.
template <typename ContextType>
class BinderMapWithContext {
 public:
  using Traits = internal::BinderContextTraits<ContextType>;
  using ContextValueType = typename Traits::ValueType;
  using GenericBinderType = typename Traits::GenericBinderType;

  template <typename Interface>
  using BinderType = typename Traits::template BinderType<Interface>;

  BinderMapWithContext() = default;
  ~BinderMapWithContext() = default;

  // Adds a new binder specifically for Interface receivers. This exists for the
  // convenience of being able to register strongly-typed binding methods like:
  //
  //   void OnBindFoo(mojo::PendingReceiver<Foo> receiver) { ... }
  //
  // more easily.
  template <typename Interface>
  void Add(BinderType<Interface> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    binders_[Interface::Name_] = std::make_unique<
        internal::GenericCallbackBinderWithContext<ContextType>>(
        Traits::MakeGenericBinder(std::move(binder)), std::move(task_runner));
  }

  // Passes |*receiver_pipe| to a registered binder for |interface_name| if any
  // exists. Returns |true| if successful, or |false| if no binder is registered
  // for the given interface. Upon returning |false|, |*receiver_pipe| is left
  // intact for the caller. Only usable when ContextType is void.
  bool TryBind(const std::string& interface_name,
               mojo::ScopedMessagePipeHandle* receiver_pipe) {
    static_assert(std::is_same<ContextType, void>::value,
                  "TryBind() must be called with a context value when "
                  "ContextType is non-void.");
    auto it = binders_.find(interface_name);
    if (it == binders_.end())
      return false;

    it->second->BindInterface(std::move(*receiver_pipe));
    return true;
  }

  // Like above, but passes |context| to the binder if one exists. Only usable
  // when ContextType is non-void.
  bool TryBind(ContextValueType context,
               const std::string& interface_name,
               mojo::ScopedMessagePipeHandle* receiver_pipe) {
    static_assert(!std::is_same<ContextType, void>::value,
                  "TryBind() must be called without a context value when "
                  "ContextType is void.");
    auto it = binders_.find(interface_name);
    if (it == binders_.end())
      return false;

    it->second->BindInterface(std::move(context), std::move(*receiver_pipe));
    return true;
  }

  // Clears all registered binders from this map.
  void Clear() { binders_.clear(); }

 private:
  std::map<
      std::string,
      std::unique_ptr<internal::GenericCallbackBinderWithContext<ContextType>>>
      binders_;

  DISALLOW_COPY_AND_ASSIGN(BinderMapWithContext);
};

// Common alias for BinderMapWithContext that has no context. Binders added to
// this type of map will only take a single PendingReceiver<T> argument (or a
// generic name+pipe combo).
using BinderMap = BinderMapWithContext<void>;

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_H_
