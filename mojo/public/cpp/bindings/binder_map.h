// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/lib/binder_map_internal.h"

namespace mojo {

// BinderMapWithContext is a helper class that maintains a registry of
// callbacks that bind receivers for arbitrary Mojo interfaces. By default the
// map is empty and cannot bind any interfaces.
//
// Call |Add()| to register a new binder for a specific interface.
// Call |TryBind()| to attempt to run a registered binder on the generic
// input receiver. If a suitable binder is found, it will take ownership of the
// receiver.
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
  BinderMapWithContext(const BinderMapWithContext&) = default;
  BinderMapWithContext(BinderMapWithContext&&) = default;
  ~BinderMapWithContext() = default;

  BinderMapWithContext& operator=(const BinderMapWithContext&) = default;
  BinderMapWithContext& operator=(BinderMapWithContext&&) = default;

  // Adds a new binder specifically for Interface receivers. This exists for the
  // convenience of being able to register strongly-typed binding methods like:
  //
  //   void OnBindFoo(mojo::PendingReceiver<Foo> receiver) { ... }
  //
  // more easily.
  //
  // If |Add()| is called multiple times for the same interface, the most
  // recent one replaces any existing binder.
  template <typename Interface>
  void Add(std::common_type_t<BinderType<Interface>> binder,
           scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    binders_[Interface::Name_] = std::make_unique<
        internal::GenericCallbackBinderWithContext<ContextType>>(
        Traits::MakeGenericBinder(std::move(binder)), std::move(task_runner));
  }

  // Returns true if this map contains a binder for `Interface` receivers.
  template <typename Interface>
  bool Contains() {
    return base::Contains(binders_, Interface::Name_);
  }

  // Attempts to bind the |receiver| using one of the registered binders in
  // this map. If a matching binder is found, ownership of the |receiver|'s
  // MessagePipe will be transferred and this will return |true|. If the binder
  // was registered with a SequencedTaskRunner, the binder will be dispatched
  // asynchronously on it; otherwise, it will be called directly.
  //
  // If no matching binder is found, this returns |false| and the |receiver|
  // will be left intact for the caller.
  //
  // This method is only usable when ContextType is void.
  [[nodiscard]] bool TryBind(mojo::GenericPendingReceiver* receiver) {
    static_assert(IsVoidContext::value,
                  "TryBind() must be called with a context value when "
                  "ContextType is non-void.");
    auto it = binders_.find(*receiver->interface_name());
    if (it == binders_.end())
      return false;

    it->second->BindInterface(receiver->PassPipe());
    return true;
  }

  // Like above, but passes |context| to the binder if one exists. Only usable
  // when ContextType is non-void.
  [[nodiscard]] bool TryBind(ContextValueType context,
                             mojo::GenericPendingReceiver* receiver) {
    static_assert(!IsVoidContext::value,
                  "TryBind() must be called without a context value when "
                  "ContextType is void.");
    auto it = binders_.find(*receiver->interface_name());
    if (it == binders_.end())
      return default_binder_ && default_binder_.Run(context, *receiver);

    it->second->BindInterface(std::move(context), receiver->PassPipe());
    return true;
  }

  // DO NOT USE. This sets a generic default handler for any receiver that
  // doesn't match a registered binder. It's a transitional API to help migrate
  // some older code to BinderMap. Reliance on this mechanism makes security
  // auditing more difficult. Note that this intentionally only supports use
  // with a non-void ContextType, since that's the only existing use case.
  using DefaultBinder =
      base::RepeatingCallback<bool(ContextValueType context,
                                   mojo::GenericPendingReceiver&)>;
  void SetDefaultBinderDeprecated(DefaultBinder binder) {
    default_binder_ = std::move(binder);
  }

  void GetInterfacesForTesting(std::vector<std::string>& out) {
    for (const auto& [key, _] : binders_) {
      out.push_back(key);
    }
  }

 private:
  using IsVoidContext = std::is_same<ContextType, void>;

  std::map<
      std::string,
      std::unique_ptr<internal::GenericCallbackBinderWithContext<ContextType>>>
      binders_;
  DefaultBinder default_binder_;
};

// Common alias for BinderMapWithContext that has no context. Binders added to
// this type of map will only take a single PendingReceiver<T> argument (or a
// GenericPendingReceiver).
class BinderMap : public BinderMapWithContext<void> {};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDER_MAP_H_
